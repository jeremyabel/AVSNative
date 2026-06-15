#include "Texer2.h"
#include "engine/JsonUtil.h"

#include "engine/FBOManager.h"
#include "engine/AudioAnalyzer.h"

#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"
#include "generated/spirv/vs_texer2_sprite.sc.bin.h"
#include "generated/spirv/fs_texer2_sprite.sc.bin.h"

#include <algorithm>
#include <chrono>
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
    "sizex","sizey","r","red","green","blue","skip",
};

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

// ── Serialize / Deserialize ───────────────────────────────────────────────────

nlohmann::json Texer2::Serialize() const
{
    nlohmann::json j = {
        { kMode,      Mode      },
        { kResize,    Resize    },
        { kWrap,      Wrap      },
        { kColorize,  Colorize  },
        { kInitCode,  InitCode  },
        { kFrameCode, FrameCode },
        { kBeatCode,  BeatCode  },
        { kPointCode, PointCode },
        // Single-mode bundle asset reference — raw bytes arrive via ApplyAsset.
        { kImageData, ImageData },
    };
    // Keyed-array entries (imageCount / imageN / imageKeys / selectedImage).
    Keyed.Serialize(j);
    return j;
}

void Texer2::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt   (j, kMode,      Mode);
    JsonUtil::ReadBool  (j, kResize,    Resize);
    JsonUtil::ReadBool  (j, kWrap,      Wrap);
    JsonUtil::ReadBool  (j, kColorize,  Colorize);
    JsonUtil::ReadString(j, kInitCode,  InitCode);
    JsonUtil::ReadString(j, kFrameCode, FrameCode);
    JsonUtil::ReadString(j, kBeatCode,  BeatCode);
    JsonUtil::ReadString(j, kPointCode, PointCode);
    JsonUtil::ReadString(j, kImageData, ImageData);
    // Keyed-array list (entries' raw bytes also arrive via ApplyAsset afterwards).
    Keyed.Deserialize(j);

    if (m_inited)
        LoadSelected();

    RecompileInitCode();
}

// ── Preset bundle assets ──────────────────────────────────────────────────────

std::vector<PresetAsset> Texer2::CollectAssets() const
{
    std::vector<PresetAsset> out;
    if (Mode == 0)
    {
        if (!m_raw.empty())
            out.push_back({ kImageData, m_name, m_raw });
    }
    else
    {
        Keyed.CollectAssets(out);
    }
    return out;
}

void Texer2::ApplyAsset(const std::string& key, const std::string& name,
                        std::vector<uint8_t> bytes)
{
    if (key == kImageData)
    {
        m_raw  = std::move(bytes);
        m_name = name;
        ImageData = name;
        if (m_inited && Mode == 0)
            LoadSelected();
        return;
    }

    // Keyed-array entry ("imageN"): route bytes into the matching list slot.
    if (Keyed.ApplyAsset(key, name, std::move(bytes)) && m_inited && Mode == 1)
        LoadSelected();
}

// Picks the active source (single-mode image, or the selected keyed image) and
// rebuilds the displayed image from it. Empty bytes → built-in soft-dot default.
void Texer2::LoadSelected()
{
    if (!m_inited)
        return;

    if (Mode == 1)
    {
        Keyed.ClampSelected();
        if (!Keyed.Images.empty())
            BuildFromRaw(Keyed.Images[Keyed.Selected].Raw);
        else
            BuildFromRaw({});  // empty list → default soft-dot
    }
    else
    {
        BuildFromRaw(m_raw);
    }
}

// ── Recompile entry points ────────────────────────────────────────────────────

void Texer2::RecompileInitCode()
{
    if (!m_inited) return;

    // Re-scan all code for user vars (init code change may add/remove vars).
    RescanAndSeed();
    m_lua.CompileBlock(SubstitutePi(InitCode),   "initCode",   m_initRef);
    m_lua.CompileBlock(SubstitutePi(FrameCode),  "frameCode",  m_frameRef);
    m_lua.CompileBlock(SubstitutePi(BeatCode),   "beatCode",   m_beatRef);
    m_lua.CompileBlock(SubstitutePi(PointCode),  "pointCode",  m_pointRef);
    RunInit();
}

void Texer2::RecompileFrameCode()
{
    if (!m_inited) return;
    m_lua.CompileBlock(SubstitutePi(FrameCode), "frameCode", m_frameRef);
}

void Texer2::RecompileBeatCode()
{
    if (!m_inited) return;
    m_lua.CompileBlock(SubstitutePi(BeatCode), "beatCode", m_beatRef);
}

void Texer2::RecompilePointCode()
{
    if (!m_inited) return;
    m_lua.CompileBlock(SubstitutePi(PointCode), "pointCode", m_pointRef);
}

// ── Default 21×21 soft-dot image ─────────────────────────────────────────────

void Texer2::MakeDefaultImage()
{
    ResetAnimation();

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
    m_spriteDirty = true;
}

// ── Animation state ───────────────────────────────────────────────────────────

void Texer2::ResetAnimation()
{
    m_frames.clear();
    m_frameDelaysMs.clear();
    m_curFrame     = 0;
    m_frameAccumMs = 0.0;
    m_haveTick     = false;
}

// Advance the displayed GIF frame by elapsed wall-clock time. No-op for static images.
void Texer2::AdvanceAnimation()
{
    if (m_frames.size() <= 1) return;

    const auto now = std::chrono::steady_clock::now();
    if (!m_haveTick) { m_lastTick = now; m_haveTick = true; return; }

    const double elapsedMs = std::chrono::duration<double, std::milli>(now - m_lastTick).count();
    m_lastTick = now;

    // Clamp huge gaps (window unfocused, breakpoint, …) so the catch-up loop can't spin.
    m_frameAccumMs += std::min(elapsedMs, 1000.0);

    const size_t prev = m_curFrame;
    size_t guard = 0;
    while (m_frameAccumMs >= (double)m_frameDelaysMs[m_curFrame] && guard++ < m_frames.size())
    {
        m_frameAccumMs -= (double)m_frameDelaysMs[m_curFrame];
        m_curFrame = (m_curFrame + 1) % m_frames.size();
    }
    if (m_curFrame != prev)
    {
        m_imgPixels = m_frames[m_curFrame];
        m_spriteDirty = true;
    }
}

// ── BuildFromRaw ──────────────────────────────────────────────────────────────

void Texer2::BuildFromRaw(const std::vector<uint8_t>& raw)
{
    ResetAnimation();
    m_spriteDirty = true;

    if (raw.empty()) { MakeDefaultImage(); return; }

    // Try the GIF loader first — it returns null for any non-GIF format. A GIF yields
    // `frames` consecutive RGBA8 images (w*h*4 each) plus a per-frame delay array (ms).
    int* delays = nullptr;
    int  w = 0, h = 0, frames = 0, ch = 0;
    uint8_t* gif = stbi_load_gif_from_memory(
        raw.data(), (int)raw.size(), &delays, &w, &h, &frames, &ch, 4);
    if (gif)
    {
        m_imgW = w; m_imgH = h;
        const size_t frameBytes = (size_t)w * h * 4;

        if (frames > 1)
        {
            m_frames.resize(frames);
            m_frameDelaysMs.resize(frames);
            for (int f = 0; f < frames; f++)
            {
                m_frames[f].assign(gif + (size_t)f * frameBytes, gif + (size_t)(f + 1) * frameBytes);
                // A 0 delay (very common) plays as fast as possible in browsers; clamp to ~10fps.
                m_frameDelaysMs[f] = (delays && delays[f] > 0) ? delays[f] : 100;
            }
            m_imgPixels = m_frames[0];
        }
        else
        {
            m_imgPixels.assign(gif, gif + frameBytes);   // single-frame GIF: treat as static
        }

        stbi_image_free(gif);
        if (delays) stbi_image_free(delays);
        return;
    }
    if (delays) stbi_image_free(delays);   // defensive; delays stays null on the non-GIF path

    // Non-GIF (PNG/JPG/BMP/TGA/…): decode the single image.
    int sw = 0, sh = 0, sch = 0;
    uint8_t* pixels = stbi_load_from_memory(raw.data(), (int)raw.size(), &sw, &sh, &sch, 4);
    if (!pixels) { MakeDefaultImage(); return; }

    m_imgW = sw; m_imgH = sh;
    m_imgPixels.assign(pixels, pixels + (size_t)sw * sh * 4);
    stbi_image_free(pixels);
}

// ── GPU sprite texture ──────────────────────────────────────────────────────────

void Texer2::EnsureImageTex()
{
    if (m_imgW <= 0 || m_imgH <= 0) return;
    if (bgfx::isValid(m_imageTex) && m_texW == m_imgW && m_texH == m_imgH) return;

    if (bgfx::isValid(m_imageTex)) {
        bgfx::destroy(m_imageTex);
        m_imageTex = BGFX_INVALID_HANDLE;
    }

    m_texW = m_imgW; m_texH = m_imgH;
    // Filtering (bilinear vs nearest) is chosen at bind time via sampler flags; the
    // texture itself just clamps.
    m_imageTex = bgfx::createTexture2D(
        (uint16_t)m_imgW, (uint16_t)m_imgH, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    m_spriteDirty = true;
}

// ── Lua helpers ───────────────────────────────────────────────────────────────

void Texer2::RescanAndSeed()
{
    // Seed built-in vars that the engine sets per-frame/per-particle.
    static const char* kVars[] = {
        "n","i","x","y","v","b","w","h","iw","ih",
        "sizex","sizey","r","red","green","blue","skip", nullptr
    };
    for (int k = 0; kVars[k]; k++)
        m_lua.SeedVar(kVars[k]);

    // Discover user-declared vars from all blocks and seed them too.
    const std::string allCode =
        InitCode + "\n" + FrameCode + "\n" + BeatCode + "\n" + PointCode;
    for (const auto& name : LuaRuntime::ScanVarDecls(allCode, k_builtins))
        m_lua.SeedVar(name);
}

void Texer2::CompileAll()
{
    m_lua.CompileBlock(SubstitutePi(InitCode),  "initCode",  m_initRef);
    m_lua.CompileBlock(SubstitutePi(FrameCode), "frameCode", m_frameRef);
    m_lua.CompileBlock(SubstitutePi(BeatCode),  "beatCode",  m_beatRef);
    m_lua.CompileBlock(SubstitutePi(PointCode), "pointCode", m_pointRef);
}

void Texer2::RunInit()
{
    m_lua.SetEnvNumber("n", 0.0);
    m_lua.RunBlock(m_initRef, "initCode");
}

// ── Per-mode GPU blend ──────────────────────────────────────────────────────────
// Maps the line blend mode to a bgfx blend state + a fragment "style" (how the sprite
// shader shapes its premultiplied output). The mode is constant for the whole frame,
// so this is computed once and reused for every particle draw. Two accepted GPU
// approximations: XOR (8) ≈ Replace, and Subtractive-2 (5) darkens transparent texels.
static void ModeBlendState(int mode, float adjAlpha,
                           uint64_t& blendOut, float& styleOut, float& paramOut)
{
    styleOut = 0.0f;   // 0 = premultiply by coverage
    paramOut = 0.0f;
    switch (mode)
    {
    case 1: // Additive
        blendOut = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE);
        break;
    case 2: // Maximum
        blendOut = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE)
                 | BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MAX);
        break;
    case 3: // 50/50 (= adjustable at alpha 0.5)
        blendOut = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        styleOut = 2.0f; paramOut = 0.5f;
        break;
    case 4: // Subtractive 1 (dst - src)
        blendOut = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE)
                 | BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_REVSUB);
        break;
    case 5: // Subtractive 2 (src - dst) — approx (transparent texels darken)
        blendOut = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE)
                 | BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_SUB);
        break;
    case 6: // Multiply
        blendOut = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_ZERO);
        styleOut = 1.0f; // mix toward white by coverage
        break;
    case 7: // Adjustable
        blendOut = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        styleOut = 2.0f; paramOut = adjAlpha;
        break;
    case 9: // Minimum
        blendOut = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE)
                 | BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MIN);
        styleOut = 1.0f;
        break;
    case 0: // Replace
    case 8: // XOR — approximated as alpha-over Replace
    default:
        blendOut = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        break;
    }
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

// Extra Lua built-ins not in LuaRuntime's default math aliases.
// (rand is provided globally by LuaRuntime — float [0,1)/[0,x)/[x,y) — so it is
// intentionally not redefined here.)
static const char k_setupCode[] = R"(
above = function(a,b) return (a > b) and 1 or 0 end
below = function(a,b) return (a < b) and 1 or 0 end
equal = function(a,b) return (a == b) and 1 or 0 end
sign  = function(n) return (n > 0) and 1 or ((n < 0) and -1 or 0) end
int   = function(n) return n >= 0 and math.floor(n) or math.ceil(n) end
atan  = function(a,b) if b ~= nil then return math.atan2(a,b) else return math.atan(a) end end
)";

void Texer2::Init()
{
    // Seed program: copy the input into the output FBO each frame (vs_fullscreen + fs_blit).
    bgfx::ShaderHandle bvs = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle bfs = bgfx::createShader(bgfx::copy(fs_blit_spv,       sizeof(fs_blit_spv)));
    m_blitProg = bgfx::createProgram(bvs, bfs, true);
    m_blitTex  = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    // Sprite program: one transformed quad per particle.
    bgfx::ShaderHandle svs = bgfx::createShader(bgfx::copy(vs_texer2_sprite_spv, sizeof(vs_texer2_sprite_spv)));
    bgfx::ShaderHandle sfs = bgfx::createShader(bgfx::copy(fs_texer2_sprite_spv, sizeof(fs_texer2_sprite_spv)));
    m_spriteProg = bgfx::createProgram(svs, sfs, true);
    m_spriteTexU = bgfx::createUniform("s_sprite", bgfx::UniformType::Sampler);
    m_xformU     = bgfx::createUniform("u_xform",  bgfx::UniformType::Vec4);
    m_rotU       = bgfx::createUniform("u_rot",    bgfx::UniformType::Vec4);
    m_colorU     = bgfx::createUniform("u_color",  bgfx::UniformType::Vec4);
    m_styleU     = bgfx::createUniform("u_style",  bgfx::UniformType::Vec4);
    m_screenU    = bgfx::createUniform("u_screen", bgfx::UniformType::Vec4);

    // Static unit quad ([-1,1] corners, two triangles); the VS scales/rotates it.
    static const float k_unitQuad[] = {
        -1.0f,-1.0f,  1.0f,-1.0f,  1.0f, 1.0f,
        -1.0f,-1.0f,  1.0f, 1.0f, -1.0f, 1.0f,
    };
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float).end();
    m_quadVB = bgfx::createVertexBuffer(bgfx::copy(k_unitQuad, sizeof(k_unitQuad)), layout);

    // Seed extra Lua built-ins (rand, above, below, equal, sign, int, atan 2-arg)
    int setupRef = -1;
    m_lua.CompileBlock(k_setupCode, "setup", setupRef);
    m_lua.RunBlock(setupRef, "setup");

    RescanAndSeed();
    CompileAll();

    m_inited = true;
    LoadSelected();  // soft-dot until ApplyAsset delivers a bundled image (if any)
    RunInit();
}

void Texer2::Destroy()
{
    if (bgfx::isValid(m_imageTex))   bgfx::destroy(m_imageTex);
    if (bgfx::isValid(m_quadVB))     bgfx::destroy(m_quadVB);
    if (bgfx::isValid(m_screenU))    bgfx::destroy(m_screenU);
    if (bgfx::isValid(m_styleU))     bgfx::destroy(m_styleU);
    if (bgfx::isValid(m_colorU))     bgfx::destroy(m_colorU);
    if (bgfx::isValid(m_rotU))       bgfx::destroy(m_rotU);
    if (bgfx::isValid(m_xformU))     bgfx::destroy(m_xformU);
    if (bgfx::isValid(m_spriteTexU)) bgfx::destroy(m_spriteTexU);
    if (bgfx::isValid(m_spriteProg)) bgfx::destroy(m_spriteProg);
    if (bgfx::isValid(m_blitTex))    bgfx::destroy(m_blitTex);
    if (bgfx::isValid(m_blitProg))   bgfx::destroy(m_blitProg);

    m_imageTex   = BGFX_INVALID_HANDLE;
    m_quadVB     = BGFX_INVALID_HANDLE;
    m_screenU    = BGFX_INVALID_HANDLE;
    m_styleU     = BGFX_INVALID_HANDLE;
    m_colorU     = BGFX_INVALID_HANDLE;
    m_rotU       = BGFX_INVALID_HANDLE;
    m_xformU     = BGFX_INVALID_HANDLE;
    m_spriteTexU = BGFX_INVALID_HANDLE;
    m_spriteProg = BGFX_INVALID_HANDLE;
    m_blitTex    = BGFX_INVALID_HANDLE;
    m_blitProg   = BGFX_INVALID_HANDLE;

    Keyed.Images.clear();
    m_texW = m_texH = 0;
    m_spriteDirty = true;
    m_inited = false;
}

// ── Render ────────────────────────────────────────────────────────────────────

void Texer2::Render(const RenderContext& Context)
{
    // Keyboard-driven image switching (key-down edge → switch selected image).
    if (Mode == 1 && Keyed.UpdateSelection())
        LoadSelected();

    AdvanceAnimation();   // step animated GIF to the current frame (no-op for static images)

    const int w = Context.Width, h = Context.Height;

    // Upload the current image to the GPU when it changed (image load / GIF frame).
    EnsureImageTex();
    if (m_spriteDirty && bgfx::isValid(m_imageTex) && !m_imgPixels.empty())
    {
        bgfx::updateTexture2D(m_imageTex, 0, 0, 0, 0, (uint16_t)m_imgW, (uint16_t)m_imgH,
            bgfx::copy(m_imgPixels.data(), (uint32_t)(m_imgW * m_imgH * 4)));
        m_spriteDirty = false;
    }

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

    const int   blendMode  = Context.LineMode ? Context.LineMode->Blend : 0;
    const float blendAlpha = (Context.LineMode ? Context.LineMode->Alpha : 0) / 255.0f;

    // One sequential view: seed the output with the input, then blend each particle
    // quad on top, in submission order, using the per-mode GPU blend.
    const uint8_t view = Context.ViewId;
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(view, Context.OutputFBO);
    bgfx::setViewRect(view, 0, 0, (uint16_t)w, (uint16_t)h);
    bgfx::setViewClear(view, BGFX_CLEAR_NONE);

    // Seed: input → output FBO.
    bgfx::setTexture(0, m_blitTex, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(view, m_blitProg);

    if (n > 0 && bgfx::isValid(m_imageTex))
    {
        uint64_t blend; float style, styleParam;
        ModeBlendState(blendMode, blendAlpha, blend, style, styleParam);
        const uint64_t drawState = BGFX_STATE_WRITE_RGB | blend;
        const uint32_t sampFlags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
            | (Resize ? 0u : (BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT));

        const float screen[4] = { (float)w, (float)h, 0.0f, 0.0f };
        const float styleU[4] = { style, styleParam, 0.0f, 0.0f };

        const float* oscData = (Context.AudioData ? Context.AudioData->osc[0] : nullptr);
        const double step = (n > 1) ? 1.0 / (double)(n - 1) : 0.0;

        // Submit one particle quad centered at pixel (ccx, ccy).
        auto drawAt = [&](double ccx, double ccy, double destW, double destH,
                          const float rotU[4], const float colU[4])
        {
            const float xf[4] = { (float)ccx, (float)ccy, (float)(destW * 0.5), (float)(destH * 0.5) };
            bgfx::setUniform(m_xformU,  xf);
            bgfx::setUniform(m_rotU,    rotU);
            bgfx::setUniform(m_colorU,  colU);
            bgfx::setUniform(m_styleU,  styleU);
            bgfx::setUniform(m_screenU, screen);
            bgfx::setTexture(0, m_spriteTexU, m_imageTex, sampFlags);
            bgfx::setState(drawState);
            bgfx::setVertexBuffer(0, m_quadVB);
            bgfx::submit(view, m_spriteProg);
        };

        for (int j = 0; j < n; j++)
        {
            m_lua.SetEnvNumber("i",     j * step);
            m_lua.SetEnvNumber("x",     0.0);
            m_lua.SetEnvNumber("y",     0.0);
            m_lua.SetEnvNumber("sizex", 1.0);
            m_lua.SetEnvNumber("sizey", 1.0);
            m_lua.SetEnvNumber("r",     0.0);
            m_lua.SetEnvNumber("red",   1.0);
            m_lua.SetEnvNumber("green", 1.0);
            m_lua.SetEnvNumber("blue",  1.0);
            m_lua.SetEnvNumber("skip",  0.0);

            // Map particle index to audio waveform sample [−1, 1].
            const int aIdx = std::min((int)((double)j * 575.0 / (double)std::max(1, n)), 575);
            const double vv = oscData ? (oscData[aIdx] / 128.0 - 1.0) : 0.0;
            m_lua.SetEnvNumber("v", vv);

            m_lua.RunBlock(m_pointRef, "pointCode");

            if (m_lua.GetEnvNumber("skip") != 0.0) continue;

            const double szx = m_lua.GetEnvNumber("sizex");
            const double szy = m_lua.GetEnvNumber("sizey");
            if (std::abs(szx) < 0.01 || std::abs(szy) < 0.01) continue;

            const double rot = m_lua.GetEnvNumber("r");   // sprite rotation, radians

            double nx = m_lua.GetEnvNumber("x");
            double ny = m_lua.GetEnvNumber("y");
            if (Wrap) {
                nx -= std::round(nx / 2.0) * 2.0;
                ny -= std::round(ny / 2.0) * 2.0;
            }

            const float cr = std::clamp((float)m_lua.GetEnvNumber("red"),   0.0f, 1.0f);
            const float cg = std::clamp((float)m_lua.GetEnvNumber("green"), 0.0f, 1.0f);
            const float cb = std::clamp((float)m_lua.GetEnvNumber("blue"),  0.0f, 1.0f);
            const float colU[4] = { Colorize ? cr : 1.0f, Colorize ? cg : 1.0f,
                                    Colorize ? cb : 1.0f, 1.0f };

            const double absSzx = std::abs(szx), absSzy = std::abs(szy);
            const double destW = Resize ? std::max(1.0, m_imgW * absSzx) : (double)m_imgW;
            const double destH = Resize ? std::max(1.0, m_imgH * absSzy) : (double)m_imgH;
            const float rotU[4] = { (float)std::cos(rot), (float)std::sin(rot),
                                    szx < 0 ? 1.0f : 0.0f, szy < 0 ? 1.0f : 0.0f };

            // ny=+1 → bottom, ny=−1 → top (AVS convention); the VS flips y for NDC.
            const double cx = (nx * 0.5 + 0.5) * (double)w;
            const double cy = (ny * 0.5 + 0.5) * (double)h;

            drawAt(cx, cy, destW, destH, rotU, colU);

            if (Wrap)
            {
                const int spriteW = Resize ? std::max(1,(int)std::round(m_imgW*absSzx)) : m_imgW;
                const int spriteH = Resize ? std::max(1,(int)std::round(m_imgH*absSzy)) : m_imgH;
                const bool ovX = (cx - spriteW/2.0 < 0) || (cx + spriteW/2.0 >= w);
                const bool ovY = (cy - spriteH/2.0 < 0) || (cy + spriteH/2.0 >= h);
                const double dX = (cx < w/2.0) ? (double)w : -(double)w;
                const double dY = (cy < h/2.0) ? (double)h : -(double)h;
                if (ovX)        drawAt(cx+dX, cy,    destW, destH, rotU, colU);
                if (ovY)        drawAt(cx,    cy+dY, destW, destH, rotU, colU);
                if (ovX && ovY) drawAt(cx+dX, cy+dY, destW, destH, rotU, colU);
            }
        }
    }

    Context.FboManager->Swap();
}
