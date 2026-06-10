#include "Texer2.h"

#include "engine/FBOManager.h"
#include "engine/AudioAnalyzer.h"

#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_texer2_comp.sc.bin.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// ── Engine vars excluded from the user-var scan ──────────────────────────────
// ScanVarDecls reports bare `name=` assignments so we can pre-seed them. These
// engine-set vars are seeded separately (RescanAndSeed) and overwritten every
// frame/particle, so they must not be reported as user vars. Math functions and
// helpers (sin, rand, …) are intentionally omitted: they're never assignment
// targets, and SeedVar is idempotent over the already-seeded functions, so
// listing them would change nothing.

const std::vector<std::string> Texer2::k_builtins = {
    "n","i","x","y","v","b","w","h","iw","ih",
    "sizex","sizey","red","green","blue","skip",
};

// ── Base64 decode ─────────────────────────────────────────────────────────────

static std::vector<uint8_t> Base64Decode(const std::string& b64)
{
    static const char kChars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> out;
    out.reserve(b64.size() / 4 * 3 + 3);
    int accum = 0, bits = 0;
    for (unsigned char c : b64) {
        if (c == '=') break;
        const char* pos = std::strchr(kChars, (char)c);
        if (!pos) continue;
        accum = (accum << 6) | (int)(pos - kChars);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)(accum >> bits));
            accum &= (1 << bits) - 1;
        }
    }
    return out;
}

// ── $pi substitution ─────────────────────────────────────────────────────────

static std::string SubstitutePi(std::string code)
{
    size_t pos = 0;
    while ((pos = code.find("$pi", pos)) != std::string::npos) {
        code.replace(pos, 3, "pi");
        pos += 2;
    }
    return code;
}

// ── Field table ───────────────────────────────────────────────────────────────

const std::vector<Field>& Texer2::Fields() const
{
    static const std::vector<Field> kFields = {
        ::Bool(&Texer2Config::Resize,    "resize",    "Resizing"),
        ::Bool(&Texer2Config::Wrap,      "wrap",      "Wrap Around"),
        ::Bool(&Texer2Config::Colorize,  "colorize",  "Color Filtering"),
        Lua  (&Texer2Config::InitCode,   "initCode",  "Init"),
        Lua  (&Texer2Config::FrameCode,  "frameCode", "Frame"),
        Lua  (&Texer2Config::BeatCode,   "beatCode",  "Beat"),
        Lua  (&Texer2Config::PointCode,  "pointCode", "Point"),
    };
    return kFields;
}

// ── GetConfig / SetConfig ─────────────────────────────────────────────────────

nlohmann::json Texer2::GetConfig() const
{
    nlohmann::json j = ReflectedEffect<Texer2Config>::GetConfig();
    j["imageData"] = Cfg.ImageData;
    return j;
}

void Texer2::SetConfig(const nlohmann::json& cfg)
{
    ReflectedEffect<Texer2Config>::SetConfig(cfg);

    if (cfg.contains("imageData") && cfg["imageData"].is_string())
    {
        const std::string newData = cfg["imageData"].get<std::string>();
        if (newData != Cfg.ImageData)
        {
            Cfg.ImageData = newData;
            if (m_inited)
                LoadImage(Cfg.ImageData);
        }
    }
}

// ── OnConfigChanged ───────────────────────────────────────────────────────────

void Texer2::OnConfigChanged(const std::vector<std::string>& changed)
{
    if (!m_inited) return;

    bool recompileInit = false;
    bool recompileFrame = false, recompileBeat = false, recompilePoint = false;

    for (const auto& key : changed)
    {
        if (key == "initCode")   recompileInit = true;
        if (key == "frameCode")  recompileFrame = true;
        if (key == "beatCode")   recompileBeat = true;
        if (key == "pointCode")  recompilePoint = true;
    }

    if (recompileInit) {
        // Re-scan all code for user vars (init code change may add/remove vars).
        RescanAndSeed();
        m_lua.CompileBlock(SubstitutePi(Cfg.InitCode),   "initCode",   m_initRef);
        m_lua.CompileBlock(SubstitutePi(Cfg.FrameCode),  "frameCode",  m_frameRef);
        m_lua.CompileBlock(SubstitutePi(Cfg.BeatCode),   "beatCode",   m_beatRef);
        m_lua.CompileBlock(SubstitutePi(Cfg.PointCode),  "pointCode",  m_pointRef);
        RunInit();
    } else {
        if (recompileFrame) m_lua.CompileBlock(SubstitutePi(Cfg.FrameCode), "frameCode", m_frameRef);
        if (recompileBeat)  m_lua.CompileBlock(SubstitutePi(Cfg.BeatCode),  "beatCode",  m_beatRef);
        if (recompilePoint) m_lua.CompileBlock(SubstitutePi(Cfg.PointCode), "pointCode", m_pointRef);
    }
}

// ── Default 21×21 soft-dot image ─────────────────────────────────────────────

void Texer2::MakeDefaultImage()
{
    const int sz = 21;
    m_imgW = m_imgH = sz;
    m_imgPixels.resize(sz * sz * 4);
    const float r = (sz - 1) / 2.0f;
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            const float dx = x - r, dy = y - r;
            const float d  = std::sqrt(dx * dx + dy * dy) / r;
            const float t  = std::max(0.0f, 1.0f - d);
            const uint8_t v = (uint8_t)(t * t * 255.0f + 0.5f);
            const int i = (y * sz + x) * 4;
            m_imgPixels[i] = m_imgPixels[i+1] = m_imgPixels[i+2] = v;
            m_imgPixels[i+3] = 255;
        }
    }
}

// ── LoadImage ─────────────────────────────────────────────────────────────────

void Texer2::LoadImage(const std::string& dataUrl)
{
    if (dataUrl.empty()) { MakeDefaultImage(); return; }

    const auto commaPos = dataUrl.find(',');
    if (commaPos == std::string::npos) { MakeDefaultImage(); return; }

    const std::vector<uint8_t> raw = Base64Decode(dataUrl.substr(commaPos + 1));
    if (raw.empty()) { MakeDefaultImage(); return; }

    int w = 0, h = 0, ch = 0;
    uint8_t* pixels = stbi_load_from_memory(raw.data(), (int)raw.size(), &w, &h, &ch, 4);
    if (!pixels) { MakeDefaultImage(); return; }

    m_imgW = w; m_imgH = h;
    m_imgPixels.assign(pixels, pixels + w * h * 4);
    stbi_image_free(pixels);
}

// ── GPU overlay buffer ────────────────────────────────────────────────────────

void Texer2::EnsureOverlay(int w, int h)
{
    if (m_bufW == w && m_bufH == h) return;

    if (bgfx::isValid(m_overlayTex)) {
        bgfx::destroy(m_overlayTex);
        m_overlayTex = BGFX_INVALID_HANDLE;
    }

    m_bufW = w; m_bufH = h;
    m_overlayBuf.assign(w * h * 4, 0);

    m_overlayTex = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
        BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
}

// ── Lua helpers ───────────────────────────────────────────────────────────────

void Texer2::RescanAndSeed()
{
    // Seed built-in vars that the engine sets per-frame/per-particle.
    static const char* kVars[] = {
        "n","i","x","y","v","b","w","h","iw","ih",
        "sizex","sizey","red","green","blue","skip", nullptr
    };
    for (int k = 0; kVars[k]; k++)
        m_lua.SeedVar(kVars[k]);

    // Discover user-declared vars from all blocks and seed them too.
    const std::string allCode =
        Cfg.InitCode + "\n" + Cfg.FrameCode + "\n" + Cfg.BeatCode + "\n" + Cfg.PointCode;
    for (const auto& name : LuaRuntime::ScanVarDecls(allCode, k_builtins))
        m_lua.SeedVar(name);
}

void Texer2::CompileAll()
{
    m_lua.CompileBlock(SubstitutePi(Cfg.InitCode),  "initCode",  m_initRef);
    m_lua.CompileBlock(SubstitutePi(Cfg.FrameCode), "frameCode", m_frameRef);
    m_lua.CompileBlock(SubstitutePi(Cfg.BeatCode),  "beatCode",  m_beatRef);
    m_lua.CompileBlock(SubstitutePi(Cfg.PointCode), "pointCode", m_pointRef);
}

void Texer2::RunInit()
{
    m_lua.SetEnvNumber("n", 0.0);
    m_lua.RunBlock(m_initRef, "initCode");
}

// ── CPU stamp ─────────────────────────────────────────────────────────────────
// Inline blend function. If the destination pixel is unpainted (alpha=0), writes
// directly. Otherwise blends according to the current line blend mode.

static inline void BlendPx(uint8_t* dst, bool painted,
                            float sr, float sg, float sb,
                            int mode, float alpha)
{
    auto clamp01 = [](float v) -> uint8_t {
        return (uint8_t)(std::min(1.0f, std::max(0.0f, v)) * 255.0f + 0.5f);
    };

    if (!painted) {
        dst[0] = clamp01(sr); dst[1] = clamp01(sg); dst[2] = clamp01(sb);
        dst[3] = 255;
        return;
    }

    const float dr = dst[0] / 255.0f, dg = dst[1] / 255.0f, db = dst[2] / 255.0f;
    float or_, og, ob;

    switch (mode) {
    case 1: or_=std::min(1.0f,dr+sr); og=std::min(1.0f,dg+sg); ob=std::min(1.0f,db+sb); break;
    case 2: or_=std::max(dr,sr); og=std::max(dg,sg); ob=std::max(db,sb); break;
    case 3: or_=(dr+sr)*0.5f; og=(dg+sg)*0.5f; ob=(db+sb)*0.5f; break;
    case 4: or_=std::max(0.0f,dr-sr); og=std::max(0.0f,dg-sg); ob=std::max(0.0f,db-sb); break;
    case 5: or_=std::max(0.0f,sr-dr); og=std::max(0.0f,sg-dg); ob=std::max(0.0f,sb-db); break;
    case 6: or_=dr*sr; og=dg*sg; ob=db*sb; break;
    case 7: or_=dr*(1.0f-alpha)+sr*alpha; og=dg*(1.0f-alpha)+sg*alpha; ob=db*(1.0f-alpha)+sb*alpha; break;
    case 8: {
        int a=(int)(dr*255+0.5f),b=(int)(dg*255+0.5f),c=(int)(db*255+0.5f);
        int x=(int)(sr*255+0.5f),y=(int)(sg*255+0.5f),z=(int)(sb*255+0.5f);
        or_=(float)(a^x)/255.0f; og=(float)(b^y)/255.0f; ob=(float)(c^z)/255.0f; break;
    }
    case 9: or_=std::min(dr,sr); og=std::min(dg,sg); ob=std::min(db,sb); break;
    default: or_=sr; og=sg; ob=sb; break;
    }

    dst[0]=clamp01(or_); dst[1]=clamp01(og); dst[2]=clamp01(ob); dst[3]=255;
}

void Texer2::StampParticle(int cx, int cy, double sizex_raw, double sizey_raw,
                            float cr, float cg, float cb, int blendMode, float alpha,
                            int bufW, int bufH)
{
    const bool flipX = sizex_raw < 0, flipY = sizey_raw < 0;
    const double szx = std::abs(sizex_raw), szy = std::abs(sizey_raw);

    int left, top, destW, destH;
    if (Cfg.Resize) {
        destW = std::max(1, (int)std::round(m_imgW * szx));
        destH = std::max(1, (int)std::round(m_imgH * szy));
        left  = (int)std::round(cx - destW * 0.5);
        top   = (int)std::round(cy - destH * 0.5);
    } else {
        destW = m_imgW; destH = m_imgH;
        left  = cx - (m_imgW >> 1);
        top   = cy - (m_imgH >> 1);
    }

    for (int dy = 0; dy < destH; dy++)
    {
        const int sy = top + dy;
        if (sy < 0 || sy >= bufH) continue;

        float fv = (destH > 1) ? (float)dy / (float)(destH - 1) : 0.0f;
        if (flipY) fv = 1.0f - fv;

        for (int dx = 0; dx < destW; dx++)
        {
            const int sx = left + dx;
            if (sx < 0 || sx >= bufW) continue;

            float fu = (destW > 1) ? (float)dx / (float)(destW - 1) : 0.0f;
            if (flipX) fu = 1.0f - fu;

            float ir, ig, ib;
            if (Cfg.Resize)
            {
                // bilinear sample
                const float tx = fu * (float)(m_imgW - 1);
                const float ty = fv * (float)(m_imgH - 1);
                const int x0 = std::max(0, (int)std::floor(tx));
                const int y0 = std::max(0, (int)std::floor(ty));
                const int x1 = std::min(x0 + 1, m_imgW - 1);
                const int y1 = std::min(y0 + 1, m_imgH - 1);
                const float ffx = tx - (float)x0, ffy = ty - (float)y0;

                auto spx = [&](int py, int px) {
                    struct { float r,g,b; } ret;
                    const int ii = (py * m_imgW + px) * 4;
                    ret.r = m_imgPixels[ii]   / 255.0f;
                    ret.g = m_imgPixels[ii+1] / 255.0f;
                    ret.b = m_imgPixels[ii+2] / 255.0f;
                    return ret;
                };
                const auto a=spx(y0,x0), b=spx(y0,x1), c=spx(y1,x0), d=spx(y1,x1);
                const float w00=(1-ffx)*(1-ffy), w10=ffx*(1-ffy), w01=(1-ffx)*ffy, w11=ffx*ffy;
                ir = a.r*w00 + b.r*w10 + c.r*w01 + d.r*w11;
                ig = a.g*w00 + b.g*w10 + c.g*w01 + d.g*w11;
                ib = a.b*w00 + b.b*w10 + c.b*w01 + d.b*w11;
            }
            else
            {
                // nearest-neighbor
                const int ix = (int)std::round(fu * (float)(m_imgW - 1));
                const int iy = (int)std::round(fv * (float)(m_imgH - 1));
                const int si = (iy * m_imgW + ix) * 4;
                ir = m_imgPixels[si]   / 255.0f;
                ig = m_imgPixels[si+1] / 255.0f;
                ib = m_imgPixels[si+2] / 255.0f;
            }

            const float sr = ir * (Cfg.Colorize ? cr : 1.0f);
            const float sg = ig * (Cfg.Colorize ? cg : 1.0f);
            const float sb = ib * (Cfg.Colorize ? cb : 1.0f);

            const int di = (sy * bufW + sx) * 4;
            BlendPx(&m_overlayBuf[di], m_overlayBuf[di + 3] > 0, sr, sg, sb, blendMode, alpha);
        }
    }
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

// Extra Lua built-ins not in LuaRuntime's default math aliases.
static const char k_setupCode[] = R"(
rand  = function(n) return math.floor(math.random() * n) end
above = function(a,b) return (a > b) and 1 or 0 end
below = function(a,b) return (a < b) and 1 or 0 end
equal = function(a,b) return (a == b) and 1 or 0 end
sign  = function(n) return (n > 0) and 1 or ((n < 0) and -1 or 0) end
int   = function(n) return n >= 0 and math.floor(n) or math.ceil(n) end
atan  = function(a,b) if b ~= nil then return math.atan2(a,b) else return math.atan(a) end end
)";

void Texer2::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv,   sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_texer2_comp_spv,  sizeof(fs_texer2_comp_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif   = bgfx::createUniform("s_input",   bgfx::UniformType::Sampler);
    m_overlayUnif = bgfx::createUniform("s_overlay", bgfx::UniformType::Sampler);
    m_paramsUnif  = bgfx::createUniform("u_t2params", bgfx::UniformType::Vec4);

    // Seed extra Lua built-ins (rand, above, below, equal, sign, int, atan 2-arg)
    int setupRef = -1;
    m_lua.CompileBlock(k_setupCode, "setup", setupRef);
    m_lua.RunBlock(setupRef, "setup");

    RescanAndSeed();
    CompileAll();

    m_inited = true;
    LoadImage(Cfg.ImageData);
    RunInit();
}

void Texer2::Destroy()
{
    if (bgfx::isValid(m_overlayTex))  bgfx::destroy(m_overlayTex);
    if (bgfx::isValid(m_paramsUnif))  bgfx::destroy(m_paramsUnif);
    if (bgfx::isValid(m_overlayUnif)) bgfx::destroy(m_overlayUnif);
    if (bgfx::isValid(m_inputUnif))   bgfx::destroy(m_inputUnif);
    if (bgfx::isValid(m_prog))        bgfx::destroy(m_prog);

    m_overlayTex  = BGFX_INVALID_HANDLE;
    m_paramsUnif  = BGFX_INVALID_HANDLE;
    m_overlayUnif = BGFX_INVALID_HANDLE;
    m_inputUnif   = BGFX_INVALID_HANDLE;
    m_prog        = BGFX_INVALID_HANDLE;

    m_bufW = m_bufH = 0;
    m_inited = false;
}

// ── Render ────────────────────────────────────────────────────────────────────

void Texer2::Render(const RenderContext& Context)
{
    const int w = Context.Width, h = Context.Height;
    EnsureOverlay(w, h);

    m_lua.SetAudioData(Context.AudioData);
    m_lua.SetEnvNumber("w",  (double)w);
    m_lua.SetEnvNumber("h",  (double)h);
    m_lua.SetEnvNumber("iw", (double)m_imgW);
    m_lua.SetEnvNumber("ih", (double)m_imgH);
    m_lua.SetEnvNumber("b",  Context.IsBeat() ? 1.0 : 0.0);

    m_lua.RunBlock(m_frameRef, "frameCode");
    if (Context.IsBeat())
        m_lua.RunBlock(m_beatRef, "beatCode");

    const int n = (int)std::clamp(m_lua.GetEnvNumber("n"), 0.0, (double)k_maxN);

    const int   blendMode = (int)((*Context.LineBlendMode) & 0xFF);
    const float blendAlpha = (float)(((*Context.LineBlendMode) >> 8) & 0xFF) / 255.0f;

    // Clear overlay
    std::fill(m_overlayBuf.begin(), m_overlayBuf.end(), (uint8_t)0);

    if (n > 0)
    {
        const float* oscData = (Context.AudioData ? Context.AudioData->osc[0] : nullptr);
        const double step = (n > 1) ? 1.0 / (double)(n - 1) : 0.0;

        for (int j = 0; j < n; j++)
        {
            m_lua.SetEnvNumber("i",     j * step);
            m_lua.SetEnvNumber("x",     0.0);
            m_lua.SetEnvNumber("y",     0.0);
            m_lua.SetEnvNumber("sizex", 1.0);
            m_lua.SetEnvNumber("sizey", 1.0);
            m_lua.SetEnvNumber("red",   1.0);
            m_lua.SetEnvNumber("green", 1.0);
            m_lua.SetEnvNumber("blue",  1.0);
            m_lua.SetEnvNumber("skip",  0.0);

            // Map particle index to audio waveform sample [−1, 1].
            const int aIdx = std::min((int)((double)j * 575.0 / (double)std::max(1, n)), 575);
            const double v = oscData ? (oscData[aIdx] / 128.0 - 1.0) : 0.0;
            m_lua.SetEnvNumber("v", v);

            m_lua.RunBlock(m_pointRef, "pointCode");

            if (m_lua.GetEnvNumber("skip") != 0.0) continue;

            const double szx = m_lua.GetEnvNumber("sizex");
            const double szy = m_lua.GetEnvNumber("sizey");
            if (std::abs(szx) < 0.01 || std::abs(szy) < 0.01) continue;

            double nx = m_lua.GetEnvNumber("x");
            double ny = m_lua.GetEnvNumber("y");

            if (Cfg.Wrap) {
                nx -= std::round(nx / 2.0) * 2.0;
                ny -= std::round(ny / 2.0) * 2.0;
            }

            const float cr = std::clamp((float)m_lua.GetEnvNumber("red"),   0.0f, 1.0f);
            const float cg = std::clamp((float)m_lua.GetEnvNumber("green"), 0.0f, 1.0f);
            const float cb = std::clamp((float)m_lua.GetEnvNumber("blue"),  0.0f, 1.0f);

            // Normalize to pixel coords.
            // ny=+1 → bottom (row h-1), ny=−1 → top (row 0) — matches original AVS convention.
            // bgfx overlay buffer is top-to-bottom so no flip needed (unlike the JS WebGL path).
            const int cx = (int)std::round((nx * 0.5 + 0.5) * (double)(w - 1));
            const int cy = (int)std::round((ny * 0.5 + 0.5) * (double)(h - 1));

            StampParticle(cx, cy, szx, szy, cr, cg, cb, blendMode, blendAlpha, w, h);

            if (Cfg.Wrap)
            {
                const double absSzx = std::abs(szx), absSzy = std::abs(szy);
                const int spriteW = Cfg.Resize ? std::max(1,(int)std::round(m_imgW*absSzx)) : m_imgW;
                const int spriteH = Cfg.Resize ? std::max(1,(int)std::round(m_imgH*absSzy)) : m_imgH;
                const bool ovX = (cx - spriteW/2 < 0) || (cx + spriteW/2 >= w);
                const bool ovY = (cy - spriteH/2 < 0) || (cy + spriteH/2 >= h);
                const int dX = (cx < w/2) ? w : -w;
                const int dY = (cy < h/2) ? h : -h;
                if (ovX)        StampParticle(cx+dX, cy,    szx, szy, cr, cg, cb, blendMode, blendAlpha, w, h);
                if (ovY)        StampParticle(cx,    cy+dY, szx, szy, cr, cg, cb, blendMode, blendAlpha, w, h);
                if (ovX && ovY) StampParticle(cx+dX, cy+dY, szx, szy, cr, cg, cb, blendMode, blendAlpha, w, h);
            }
        }
    }

    // Upload overlay to GPU
    bgfx::updateTexture2D(m_overlayTex, 0, 0, 0, 0, (uint16_t)w, (uint16_t)h,
        bgfx::copy(m_overlayBuf.data(), (uint32_t)(w * h * 4)));

    // Composite: background (input) + overlay → output FBO
    float params[4] = { (float)blendMode, blendAlpha, 0.0f, 0.0f };
    bgfx::setUniform(m_paramsUnif, params);
    bgfx::setTexture(0, m_inputUnif,   Context.InputTexture);
    bgfx::setTexture(1, m_overlayUnif, m_overlayTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, m_prog);

    Context.FboManager->Swap();
}
