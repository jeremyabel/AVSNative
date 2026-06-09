#include "SuperScope.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_superscope_blend.sc.bin.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// Built-in variable names that ScanVarDecls must not treat as user-declared.
static const std::vector<std::string> k_builtins = {
    "n","b","x","y","i","v","w","h",
    "red","green","blue","linesize","skip","drawmode",
    "getspec","getosc",
    // math aliases are excluded by LuaRuntime::GetUserVars already
};

static constexpr int kMaxPoints = 128 * 1024;

static const char* k_defaultInit  = "n = 800";
static const char* k_defaultFrame = "t = t - 0.05";
static const char* k_defaultBeat  = "";
static const char* k_defaultPoint =
    "d = i + v * 0.2\n"
    "r = t + i * pi * 4\n"
    "x = cos(r) * d\n"
    "y = sin(r) * d";

// ── Init / Destroy ────────────────────────────────────────────────────────────

void SuperScope::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    const bgfx::ShaderHandle vs = bgfx::createShader(
        bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle fs = bgfx::createShader(
        bgfx::copy(fs_superscope_blend_spv, sizeof(fs_superscope_blend_spv)));
    m_program  = bgfx::createProgram(vs, fs, true);
    m_uBase    = bgfx::createUniform("s_base",    bgfx::UniformType::Sampler);
    m_uOverlay = bgfx::createUniform("s_overlay", bgfx::UniformType::Sampler);
    m_uParams  = bgfx::createUniform("u_ssParams",bgfx::UniformType::Vec4);

    Cfg.Colors      = { {255, 255, 255} };
    Cfg.InitCode    = k_defaultInit;
    Cfg.FrameCode   = k_defaultFrame;
    Cfg.BeatCode    = k_defaultBeat;
    Cfg.PointCode   = k_defaultPoint;

    m_audioBuf.resize(kAudioBins, 128.0f);
    m_outBuf.resize(kAudioBins * kOutStride, 0.0f);

    // Seed built-ins and compile all blocks.
    for (const auto& v : k_builtins) m_lua.SeedVar(v);
    m_lua.SetEnvNumber("n", 800);
    m_lua.SetEnvNumber("drawmode", (double)Cfg.DrawMode);

    m_lua.CompileBlock(Cfg.InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
    m_lua.CompileBlock(Cfg.BeatCode,  "beatCode",  m_beatRef);

    // Seed all user-declared vars to 0 so frame/point code can reference them
    // before init has a chance to set them (e.g. `t = t - 0.05` with t unset).
    SeedUserVars();

    // Run init to discover user vars, then build the point loop.
    m_lua.RunBlock(m_initRef, "initCode");
    m_inited = true;
    RebuildPointLoop();
}

void SuperScope::Destroy()
{
    if (bgfx::isValid(m_overlayTex)) bgfx::destroy(m_overlayTex);
    if (bgfx::isValid(m_uParams))    bgfx::destroy(m_uParams);
    if (bgfx::isValid(m_uOverlay))   bgfx::destroy(m_uOverlay);
    if (bgfx::isValid(m_uBase))      bgfx::destroy(m_uBase);
    if (bgfx::isValid(m_program))    bgfx::destroy(m_program);

    m_overlayTex = BGFX_INVALID_HANDLE;
    m_uParams    = BGFX_INVALID_HANDLE;
    m_uOverlay   = BGFX_INVALID_HANDLE;
    m_uBase      = BGFX_INVALID_HANDLE;
    m_program    = BGFX_INVALID_HANDLE;
    // Lua refs are freed when m_lua (LuaRuntime member) destructs and closes lua_State.
}

// ── Color cycling ─────────────────────────────────────────────────────────────

SuperScope::Rgb SuperScope::AdvanceColor()
{
    if (Cfg.Colors.empty()) return { 1.f, 1.f, 1.f };
    if (Cfg.Colors.size() == 1)
        return { Cfg.Colors[0][0] / 255.f, Cfg.Colors[0][1] / 255.f, Cfg.Colors[0][2] / 255.f };

    const int total = (int)Cfg.Colors.size() * 64;
    m_colorPos = (m_colorPos + 1) % total;
    const int   p    = m_colorPos / 64;
    const float frac = (float)(m_colorPos % 64);
    const auto& c1 = Cfg.Colors[p];
    const auto& c2 = Cfg.Colors[(p + 1) % (int)Cfg.Colors.size()];
    return {
        (c1[0] * (63.f - frac) + c2[0] * frac) / (63.f * 255.f),
        (c1[1] * (63.f - frac) + c2[1] * frac) / (63.f * 255.f),
        (c1[2] * (63.f - frac) + c2[2] * frac) / (63.f * 255.f),
    };
}

// ── Overlay texture ───────────────────────────────────────────────────────────

void SuperScope::EnsureOverlay(int w, int h)
{
    if (m_overlayW == w && m_overlayH == h) return;
    if (bgfx::isValid(m_overlayTex)) bgfx::destroy(m_overlayTex);

    m_overlayW   = w;
    m_overlayH   = h;
    m_overlayBuf.assign(w * h * 4, 0);

    m_overlayTex = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
}

// ── Audio sampling ────────────────────────────────────────────────────────────

void SuperScope::BuildAudioBuf(const RenderContext& ctx)
{
    const VisData* vd = ctx.AudioData;
    for (int i = 0; i < kAudioBins; ++i)
    {
        float val;
        if (Cfg.AudioSource == 0) // waveform
        {
            if (Cfg.AudioChannel == 1)      val = vd ? vd->osc[0][i] : 128.f;
            else if (Cfg.AudioChannel == 2) val = vd ? vd->osc[1][i] : 128.f;
            else                          val = vd ? (vd->osc[0][i] + vd->osc[1][i]) * 0.5f : 128.f;
        }
        else // spectrum
        {
            if (Cfg.AudioChannel == 1)      val = vd ? vd->spec[0][i] : 0.f;
            else if (Cfg.AudioChannel == 2) val = vd ? vd->spec[1][i] : 0.f;
            else                          val = vd ? (vd->spec[0][i] + vd->spec[1][i]) * 0.5f : 0.f;

            // Spec is [0,255]; JS returns v = spec/255 ∈ [0,1].
            // The Lua wrapper computes v = audio[i]/128-1, so pack so that
            // (val/128 - 1) == spec/255:  val = spec*(128/255) + 128.
            val = val * (128.f / 255.f) + 128.f;
        }
        m_audioBuf[i] = val;
    }
}

// ── CPU line drawing ──────────────────────────────────────────────────────────

void SuperScope::SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= m_overlayW || y < 0 || y >= m_overlayH) return;
    int idx = (y * m_overlayW + x) * 4;
    m_overlayBuf[idx+0] = r;
    m_overlayBuf[idx+1] = g;
    m_overlayBuf[idx+2] = b;
    m_overlayBuf[idx+3] = 255;
}

void SuperScope::DrawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b)
{
    // Bresenham's line algorithm.
    int dx =  std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;)
    {
        SetPixel(x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ── User variable seeding ─────────────────────────────────────────────────────

void SuperScope::SeedUserVars()
{
    // Scan all code blocks for bare assignments and seed each declared var to 0
    // (SeedVar is a no-op for vars already set, so init-code assignments win).
    const std::string allCode =
        Cfg.InitCode + "\n" + Cfg.FrameCode + "\n" + Cfg.BeatCode + "\n" + Cfg.PointCode;
    const auto decls = LuaRuntime::ScanVarDecls(allCode, k_builtins);
    printf("[SuperScope] SeedUserVars: seeding %d vars:", (int)decls.size());
    for (const auto& v : decls) { printf(" %s", v.c_str()); m_lua.SeedVar(v); }
    printf("\n");
}

// ── Point-loop wrapper rebuild ────────────────────────────────────────────────

void SuperScope::RebuildPointLoop()
{
    // Collect user-declared vars after init has run (they're now in env).
    const std::vector<std::string> userVars = m_lua.GetUserVars();
    m_lua.CompilePointLoop(Cfg.PointCode, userVars, m_pointRef);
}

// ── Render ────────────────────────────────────────────────────────────────────

void SuperScope::Render(const RenderContext& ctx)
{
    EnsureOverlay(ctx.Width, ctx.Height);

    m_lua.SetAudioData(ctx.AudioData);

    const Rgb color = AdvanceColor();
    m_lua.SetEnvNumber("b",        ctx.IsBeat() ? 1.0 : 0.0);
    m_lua.SetEnvNumber("w",        (double)ctx.Width);
    m_lua.SetEnvNumber("h",        (double)ctx.Height);
    m_lua.SetEnvNumber("red",      color.r);
    m_lua.SetEnvNumber("green",    color.g);
    m_lua.SetEnvNumber("blue",     color.b);
    m_lua.SetEnvNumber("linesize", 1.0);
    m_lua.SetEnvNumber("drawmode", (double)Cfg.DrawMode);

    if (!m_inited)
    {
        m_lua.RunBlock(m_initRef, "initCode");
        m_inited = true;
    }
    m_lua.RunBlock(m_frameRef, "frameCode");
    if (ctx.IsBeat()) m_lua.RunBlock(m_beatRef, "beatCode");

    // Run the point loop.
    const int n = (int)std::clamp((int)m_lua.GetEnvNumber("n"), 1, kMaxPoints);
    if ((int)m_outBuf.size() < n * kOutStride)
        m_outBuf.resize(n * kOutStride, 0.f);

    // Ensure audioBuf is at least n entries; extra entries stay at 128.0f (silence)
    // so Lua audio[_i] reads don't go out of bounds when n > kAudioBins.
    if ((int)m_audioBuf.size() < n)
        m_audioBuf.resize(n, 128.0f);
    BuildAudioBuf(ctx);
    m_lua.RunPointLoop(m_pointRef, n, ctx.IsBeat(), ctx.Width, ctx.Height,
                       m_audioBuf.data(), m_outBuf.data(), "pointCode");

    // Draw into the overlay buffer.
    std::memset(m_overlayBuf.data(), 0, m_overlayBuf.size());

    int prevPx = 0, prevPy = 0;
    bool hadPrev = false;

    const float fW = (float)ctx.Width;
    const float fH = (float)ctx.Height;

    for (int pi = 0; pi < n; ++pi)
    {
        const float* pt = m_outBuf.data() + pi * kOutStride;
        const float fx = pt[0], fy = pt[1];
        const float fr = pt[2], fg = pt[3], fb = pt[4];
        const float fskip = pt[5], fdraw = pt[6];

        if (fskip > 0.0f) { hadPrev = false; continue; }

        // Guard against NaN or very large values from Lua scripts; either would
        // overflow (int) cast and produce extreme px/py that hang DrawLine.
        if (!std::isfinite(fx) || !std::isfinite(fy)) { hadPrev = false; continue; }

        const uint8_t r = (uint8_t)std::clamp((int)(fr * 255.f + 0.5f), 0, 255);
        const uint8_t g = (uint8_t)std::clamp((int)(fg * 255.f + 0.5f), 0, 255);
        const uint8_t b = (uint8_t)std::clamp((int)(fb * 255.f + 0.5f), 0, 255);

        // AVS convention: x,y in [-1,+1], y=-1=top, y=+1=bottom.
        // Clamp keeps values within a range that DrawLine can traverse quickly.
        const int px = (int)std::clamp((fx + 1.0f) * 0.5f * fW, -fW, fW * 2.0f);
        const int py = (int)std::clamp((fy + 1.0f) * 0.5f * fH, -fH, fH * 2.0f);

        if (fdraw > 0.0f && hadPrev)
            DrawLine(prevPx, prevPy, px, py, r, g, b);
        else
            SetPixel(px, py, r, g, b);

        prevPx = px; prevPy = py; hadPrev = true;
    }

    // Upload overlay CPU buffer to bgfx texture.
    bgfx::updateTexture2D(
        m_overlayTex, 0, 0, 0, 0,
        (uint16_t)ctx.Width, (uint16_t)ctx.Height,
        bgfx::copy(m_overlayBuf.data(), (uint32_t)m_overlayBuf.size()));

    // Composite overlay onto the current frame using SetRenderMode blend.
    const uint32_t lbm       = ctx.LineBlendMode ? *ctx.LineBlendMode : (1u << 16);
    const float    blendMode = (float)(lbm & 0xFF);
    const float    alpha     = (float)((lbm >> 8) & 0xFF);
    const float params[4] = { blendMode, alpha, 0.f, 0.f };
    bgfx::setUniform(m_uParams, params);
    bgfx::setTexture(0, m_uBase,    ctx.InputTexture);
    bgfx::setTexture(1, m_uOverlay, m_overlayTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, ctx.QuadVB);
    bgfx::submit(ctx.ViewId, m_program);

    ctx.FboManager->Swap();
}

// ── Script error ──────────────────────────────────────────────────────────────

std::string SuperScope::GetScriptError(const std::string& paramName) const
{
    return m_lua.GetError(paramName);
}
