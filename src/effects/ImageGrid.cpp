#include "ImageGrid.h"

#include "engine/FBOManager.h"

#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_imagegrid.sc.bin.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

// ── Engine-set built-ins, excluded from the user-var scan ─────────────────────
// x,y,sizex,sizey,r persist across frames (set in Init, mutated in Frame/Beat);
// width,height,b are written every frame. All are seeded separately, so they must
// not be reported as user vars by ScanVarDecls.
const std::vector<std::string> ImageGrid::k_builtins = {
    "x", "y", "sizex", "sizey", "r", "width", "height", "b",
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

// ── GetConfig / SetConfig ─────────────────────────────────────────────────────

nlohmann::json ImageGrid::GetConfig() const
{
    nlohmann::json j = ReflectedEffect<ImageGridConfig>::GetConfig();
    j["imageData"] = Cfg.ImageData;
    return j;
}

void ImageGrid::SetConfig(const nlohmann::json& cfg)
{
    ReflectedEffect<ImageGridConfig>::SetConfig(cfg);

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

void ImageGrid::OnConfigChanged(const std::vector<std::string>& changed)
{
    if (!m_inited) return;

    bool recompileInit = false, recompileFrame = false, recompileBeat = false;
    for (const auto& key : changed)
    {
        if (key == "initCode")  recompileInit = true;
        if (key == "frameCode") recompileFrame = true;
        if (key == "beatCode")  recompileBeat = true;
    }

    if (recompileInit)
    {
        // An init-code change may add/remove user vars: re-scan + recompile everything.
        RescanAndSeed();
        CompileAll();
        RunInit();
    }
    else
    {
        if (recompileFrame) m_lua.CompileBlock(SubstitutePi(Cfg.FrameCode), "frameCode", m_frameRef);
        if (recompileBeat)  m_lua.CompileBlock(SubstitutePi(Cfg.BeatCode),  "beatCode",  m_beatRef);
    }
}

// ── Default 64×64 checkerboard ────────────────────────────────────────────────

void ImageGrid::MakeDefaultImage()
{
    ResetAnimation();
    const int sz = 64, cell = 8;
    m_imgW = m_imgH = sz;
    std::vector<uint8_t> px(sz * sz * 4);
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++)
        {
            const bool on = (((x / cell) + (y / cell)) & 1) != 0;
            const uint8_t v = on ? 200 : 60;
            const int i = (y * sz + x) * 4;
            px[i] = px[i+1] = px[i+2] = v;
            px[i+3] = 255;
        }

    if (bgfx::isValid(m_imageTex)) { bgfx::destroy(m_imageTex); m_imageTex = BGFX_INVALID_HANDLE; }
    // Default sampler flags (0) = repeat wrap + bilinear, which is what tiling wants.
    m_imageTex = bgfx::createTexture2D((uint16_t)sz, (uint16_t)sz, false, 1,
        bgfx::TextureFormat::RGBA8, 0,
        bgfx::copy(px.data(), (uint32_t)px.size()));
}

// ── Animation state ───────────────────────────────────────────────────────────

void ImageGrid::ResetAnimation()
{
    for (auto& h : m_gpuFrames)
        if (bgfx::isValid(h)) bgfx::destroy(h);
    m_gpuFrames.clear();
    m_frameDelaysMs.clear();
    m_curFrame     = 0;
    m_frameAccumMs = 0.0;
    m_haveTick     = false;
}

// Advance m_curFrame by wall-clock time. No GPU work — Render() reads m_gpuFrames[m_curFrame].
void ImageGrid::AdvanceAnimation()
{
    if (m_gpuFrames.size() <= 1) return;

    const auto now = std::chrono::steady_clock::now();
    if (!m_haveTick) { m_lastTick = now; m_haveTick = true; return; }

    const double elapsedMs = std::chrono::duration<double, std::milli>(now - m_lastTick).count();
    m_lastTick = now;

    // Clamp huge gaps (window unfocused, breakpoint, …) so the catch-up loop can't spin.
    m_frameAccumMs += std::min(elapsedMs, 1000.0);

    size_t guard = 0;
    while (m_frameAccumMs >= (double)m_frameDelaysMs[m_curFrame] && guard++ < m_gpuFrames.size())
    {
        m_frameAccumMs -= (double)m_frameDelaysMs[m_curFrame];
        m_curFrame = (m_curFrame + 1) % m_gpuFrames.size();
    }
}

// ── LoadImage (GPU texture, repeat wrap for seamless tiling) ──────────────────

void ImageGrid::LoadImage(const std::string& dataUrl)
{
    ResetAnimation();

    if (dataUrl.empty()) { MakeDefaultImage(); return; }

    const auto commaPos = dataUrl.find(',');
    if (commaPos == std::string::npos) { MakeDefaultImage(); return; }

    const std::vector<uint8_t> raw = Base64Decode(dataUrl.substr(commaPos + 1));
    if (raw.empty()) { MakeDefaultImage(); return; }

    // Try the GIF loader first — returns null for any non-GIF format.
    int* delays = nullptr;
    int  w = 0, h = 0, frames = 0, ch = 0;
    uint8_t* gif = stbi_load_gif_from_memory(
        raw.data(), (int)raw.size(), &delays, &w, &h, &frames, &ch, 4);
    if (gif)
    {
        m_imgW = w; m_imgH = h;
        const size_t frameBytes = (size_t)w * h * 4;

        if (bgfx::isValid(m_imageTex)) { bgfx::destroy(m_imageTex); m_imageTex = BGFX_INVALID_HANDLE; }

        if (frames > 1)
        {
            // Pre-load every frame as a separate GPU texture. AdvanceAnimation() just
            // updates m_curFrame; Render() binds m_gpuFrames[m_curFrame] directly.
            m_gpuFrames.resize(frames);
            m_frameDelaysMs.resize(frames);
            for (int f = 0; f < frames; f++)
            {
                m_gpuFrames[f] = bgfx::createTexture2D((uint16_t)w, (uint16_t)h, false, 1,
                    bgfx::TextureFormat::RGBA8, 0,
                    bgfx::copy(gif + (size_t)f * frameBytes, (uint32_t)frameBytes));
                // A 0 delay plays as fast as possible in browsers; clamp to ~10fps.
                m_frameDelaysMs[f] = (delays && delays[f] > 0) ? delays[f] : 100;
            }
        }
        else
        {
            // Single-frame GIF: treat as a static image, use m_imageTex.
            m_imageTex = bgfx::createTexture2D((uint16_t)w, (uint16_t)h, false, 1,
                bgfx::TextureFormat::RGBA8, 0,
                bgfx::copy(gif, (uint32_t)frameBytes));
        }

        stbi_image_free(gif);
        if (delays) stbi_image_free(delays);
        return;
    }
    if (delays) stbi_image_free(delays);

    // Non-GIF (PNG/JPG/BMP/TGA/…): decode the single image.
    int sw = 0, sh = 0, sch = 0;
    uint8_t* pixels = stbi_load_from_memory(raw.data(), (int)raw.size(), &sw, &sh, &sch, 4);
    if (!pixels) { MakeDefaultImage(); return; }

    if (bgfx::isValid(m_imageTex)) { bgfx::destroy(m_imageTex); m_imageTex = BGFX_INVALID_HANDLE; }

    m_imgW = sw; m_imgH = sh;
    const bgfx::Memory* mem = bgfx::copy(pixels, (uint32_t)(sw * sh * 4));
    stbi_image_free(pixels);

    // Repeat wrap (default flags) so fract() tiling has no edge seams.
    m_imageTex = bgfx::createTexture2D((uint16_t)sw, (uint16_t)sh, false, 1,
        bgfx::TextureFormat::RGBA8, 0, mem);
}

// ── Lua helpers ───────────────────────────────────────────────────────────────

void ImageGrid::RescanAndSeed()
{
    for (const auto& name : k_builtins)
        m_lua.SeedVar(name);

    const std::string allCode =
        Cfg.InitCode + "\n" + Cfg.FrameCode + "\n" + Cfg.BeatCode;
    for (const auto& name : LuaRuntime::ScanVarDecls(allCode, k_builtins))
        m_lua.SeedVar(name);
}

void ImageGrid::CompileAll()
{
    m_lua.CompileBlock(SubstitutePi(Cfg.InitCode),  "initCode",  m_initRef);
    m_lua.CompileBlock(SubstitutePi(Cfg.FrameCode), "frameCode", m_frameRef);
    m_lua.CompileBlock(SubstitutePi(Cfg.BeatCode),  "beatCode",  m_beatRef);
}

void ImageGrid::RunInit()
{
    m_lua.RunBlock(m_initRef, "initCode");
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

// Extra Lua built-ins not in LuaRuntime's default math aliases (matches Texer2).
static const char k_setupCode[] = R"(
rand  = function(n) return math.floor(math.random() * n) end
above = function(a,b) return (a > b) and 1 or 0 end
below = function(a,b) return (a < b) and 1 or 0 end
equal = function(a,b) return (a == b) and 1 or 0 end
sign  = function(n) return (n > 0) and 1 or ((n < 0) and -1 or 0) end
int   = function(n) return n >= 0 and math.floor(n) or math.ceil(n) end
atan  = function(a,b) if b ~= nil then return math.atan2(a,b) else return math.atan(a) end end
)";

void ImageGrid::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_imagegrid_spv, sizeof(fs_imagegrid_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif  = bgfx::createUniform("s_input",      bgfx::UniformType::Sampler);
    m_imageUnif  = bgfx::createUniform("s_image",      bgfx::UniformType::Sampler);
    m_xformUnif  = bgfx::createUniform("u_gridXform",  bgfx::UniformType::Vec4);
    m_paramsUnif = bgfx::createUniform("u_gridParams", bgfx::UniformType::Vec4);

    // Seed extra Lua built-ins (rand, above, below, equal, sign, int, atan 2-arg).
    int setupRef = -1;
    m_lua.CompileBlock(k_setupCode, "setup", setupRef);
    m_lua.RunBlock(setupRef, "setup");

    RescanAndSeed();
    CompileAll();

    m_inited = true;
    LoadImage(Cfg.ImageData);
    RunInit();
}

void ImageGrid::Destroy()
{
    for (auto& h : m_gpuFrames)
        if (bgfx::isValid(h)) bgfx::destroy(h);
    m_gpuFrames.clear();
    if (bgfx::isValid(m_imageTex))   bgfx::destroy(m_imageTex);
    if (bgfx::isValid(m_paramsUnif)) bgfx::destroy(m_paramsUnif);
    if (bgfx::isValid(m_xformUnif))  bgfx::destroy(m_xformUnif);
    if (bgfx::isValid(m_imageUnif))  bgfx::destroy(m_imageUnif);
    if (bgfx::isValid(m_inputUnif))  bgfx::destroy(m_inputUnif);
    if (bgfx::isValid(m_prog))       bgfx::destroy(m_prog);

    m_imageTex   = BGFX_INVALID_HANDLE;
    m_paramsUnif = BGFX_INVALID_HANDLE;
    m_xformUnif  = BGFX_INVALID_HANDLE;
    m_imageUnif  = BGFX_INVALID_HANDLE;
    m_inputUnif  = BGFX_INVALID_HANDLE;
    m_prog       = BGFX_INVALID_HANDLE;
    m_imgW = m_imgH = 0;
    m_inited = false;
}

// ── Render ────────────────────────────────────────────────────────────────────

void ImageGrid::Render(const RenderContext& Ctx)
{
    AdvanceAnimation();

    const int w = Ctx.Width, h = Ctx.Height;

    // Run per-frame Lua. width/height/b are engine-set; x/y/sizex/sizey/r persist.
    m_lua.SetAudioData(Ctx.AudioData);
    m_lua.SetEnvNumber("width",  (double)w);
    m_lua.SetEnvNumber("height", (double)h);
    m_lua.SetEnvNumber("b",      Ctx.IsBeat() ? 1.0 : 0.0);

    m_lua.RunBlock(m_frameRef, "frameCode");
    if (Ctx.IsBeat())
        m_lua.RunBlock(m_beatRef, "beatCode");

    const float x     = (float)m_lua.GetEnvNumber("x");
    const float y     = (float)m_lua.GetEnvNumber("y");
    const float sizex = (float)m_lua.GetEnvNumber("sizex");
    const float sizey = (float)m_lua.GetEnvNumber("sizey");
    const float r     = (float)m_lua.GetEnvNumber("r");

    const float imgAspect = (m_imgH > 0) ? (float)m_imgW / (float)m_imgH : 1.0f;
    const float scrAspect = (h > 0) ? (float)w / (float)h : 1.0f;

    float xform[4]  = { x, y, sizex, sizey };
    float params[4] = { r, imgAspect, scrAspect, (float)Cfg.BlendMode };
    bgfx::setUniform(m_xformUnif,  xform);
    bgfx::setUniform(m_paramsUnif, params);

    const bgfx::TextureHandle curTex =
        m_gpuFrames.empty() ? m_imageTex : m_gpuFrames[m_curFrame];

    bgfx::setTexture(0, m_inputUnif, Ctx.InputTexture);
    bgfx::setTexture(1, m_imageUnif, curTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Ctx.QuadVB);
    bgfx::submit(Ctx.ViewId, m_prog);

    Ctx.FboManager->Swap();
}
