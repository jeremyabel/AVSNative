#include "Simple.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <nanovg/nanovg.h>
#include <nanovg/nanovg_bgfx.h>

#include <algorithm>
#include <cmath>

void Simple::Init()
{
    const bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle fs = bgfx::createShader(bgfx::copy(fs_simple_spv,    sizeof(fs_simple_spv)));
    m_program        = bgfx::createProgram(vs, fs, true);
    m_inputSampler   = bgfx::createUniform("s_input",       bgfx::UniformType::Sampler);
    m_overlaySampler = bgfx::createUniform("s_overlay",     bgfx::UniformType::Sampler);
    m_paramsUniform  = bgfx::createUniform("u_simpleParams", bgfx::UniformType::Vec4);

    EnsureNvgContext();
}

void Simple::EnsureNvgContext()
{
    if (m_nvg && m_nvgEdgeAa == Cfg.AntialiasingEnabled)
        return;

    // edgeaa is baked into the context at creation time, so recreate when it changes.
    DestroyOverlay();
    if (m_nvg)
        nvgDelete(m_nvg);

    m_nvgEdgeAa = Cfg.AntialiasingEnabled;
    m_nvg = nvgCreate(Cfg.AntialiasingEnabled ? 1 : 0, 0);
}

void Simple::EnsureOverlay(int Width, int Height)
{
    if (m_overlayW == Width && m_overlayH == Height)
        return;

    DestroyOverlay();
    m_overlayFbo = nvgluCreateFramebuffer(m_nvg, Width, Height, 0);
    m_overlayW   = Width;
    m_overlayH   = Height;
}

void Simple::DestroyOverlay()
{
    if (m_overlayFbo)
    {
        nvgluDeleteFramebuffer(m_overlayFbo);
        m_overlayFbo = nullptr;
    }
    m_overlayW = 0;
    m_overlayH = 0;
}

std::array<float, 3> Simple::GetCurrentColor()
{
    if (Cfg.Colors.empty())
        return { 1.0f, 1.0f, 1.0f };

    if (Cfg.Colors.size() == 1)
        return { Cfg.Colors[0][0] / 255.0f, Cfg.Colors[0][1] / 255.0f, Cfg.Colors[0][2] / 255.0f };

    const int total = (int)Cfg.Colors.size() * 64;
    m_colorPos = (m_colorPos + 1) % total;
    const int seg  = m_colorPos / 64;
    const float t  = (m_colorPos % 64) / 64.0f;
    const auto& c0 = Cfg.Colors[seg % Cfg.Colors.size()];
    const auto& c1 = Cfg.Colors[(seg + 1) % Cfg.Colors.size()];
    return {
        (c0[0] * (1.0f - t) + c1[0] * t) / 255.0f,
        (c0[1] * (1.0f - t) + c1[1] * t) / 255.0f,
        (c0[2] * (1.0f - t) + c1[2] * t) / 255.0f,
    };
}

void Simple::Render(const RenderContext& Context)
{
    const int W  = Context.Width;
    const int H  = Context.Height;
    const int h2 = H / 2;

    EnsureNvgContext();
    EnsureOverlay(W, H);
    if (!m_overlayFbo)
        return;

    // ── View N : NanoVG overlay pass ─────────────────────────────────────────
    const uint8_t nvgView = Context.ViewId;

    nvgluSetViewFramebuffer(nvgView, m_overlayFbo);  // points view at overlay, sets sequential mode
    bgfx::setViewClear(nvgView, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(nvgView, 0, 0, (uint16_t)W, (uint16_t)H);

    nvgluBindFramebuffer(m_overlayFbo);
    nvgBeginFrame(m_nvg, (float)W, (float)H, 1.0f);

    if (Context.AudioData)
    {
        const VisData& vis = *Context.AudioData;

        // Select audio channel
        float spec[kAudioBins], osc[kAudioBins];
        if (Cfg.Channel == 2)
        {
            for (int i = 0; i < kAudioBins; i++)
            {
                spec[i] = (vis.spec[0][i] + vis.spec[1][i]) * 0.5f;
                osc[i]  = (vis.osc[0][i]  + vis.osc[1][i])  * 0.5f;
            }
        }
        else
        {
            for (int i = 0; i < kAudioBins; i++)
            {
                spec[i] = vis.spec[Cfg.Channel][i];
                osc[i]  = vis.osc[Cfg.Channel][i];
            }
        }

        float yBase;
        if (Cfg.Position == 0)      yBase = 0.0f;
        else if (Cfg.Position == 2) yBase = (float)(H - 1);
        else                    yBase = (float)h2;

        const uint32_t lbm       = Context.LineBlendMode ? *Context.LineBlendMode : (1u << 16);
        const float    lineWidth = std::max(1.0f, (float)((lbm >> 16) & 0xFF));

        const auto [cr, cg, cb] = GetCurrentColor();
        nvgStrokeColor(m_nvg, nvgRGBAf(cr, cg, cb, 1.0f));
        nvgFillColor(m_nvg,   nvgRGBAf(cr, cg, cb, 1.0f));
        nvgStrokeWidth(m_nvg, lineWidth);

        const float xscale = (float)kAudioBins / W;

        if (Cfg.Mode == 0 || Cfg.Mode == 1)
        {
            // Analyzer modes: spectrum data
            if (Cfg.Mode == 0)
            {
                // Solid analyzer: filled polygon from baseline through spectrum top
                nvgBeginPath(m_nvg);
                nvgMoveTo(m_nvg, 0.0f, yBase);
                for (int x = 0; x < W; x++)
                {
                    const float r2   = x * xscale;
                    const int   lo   = (int)r2;
                    const int   hi   = std::min(lo + 1, kAudioBins - 1);
                    const float frac = r2 - lo;
                    const float val  = spec[lo] * (1.0f - frac) + spec[hi] * frac;
                    const float y    = yBase - val / 255.0f * h2;
                    nvgLineTo(m_nvg, (float)x, y);
                }
                nvgLineTo(m_nvg, (float)(W - 1), yBase);
                nvgClosePath(m_nvg);
                nvgFill(m_nvg);
            }
            else
            {
                // Line analyzer: connected polyline along spectrum top
                nvgBeginPath(m_nvg);
                for (int x = 0; x < W; x++)
                {
                    const float r2   = x * xscale;
                    const int   lo   = (int)r2;
                    const int   hi   = std::min(lo + 1, kAudioBins - 1);
                    const float frac = r2 - lo;
                    const float val  = spec[lo] * (1.0f - frac) + spec[hi] * frac;
                    const float y    = yBase - val / 255.0f * h2;
                    if (x == 0) nvgMoveTo(m_nvg, 0.5f, y);
                    else        nvgLineTo(m_nvg, x + 0.5f, y);
                }
                nvgStroke(m_nvg);
            }
        }
        else
        {
            // Scope modes: waveform data (128 = zero-crossing)
            if (Cfg.Mode == 2)
            {
                // Line scope: connected polyline
                nvgBeginPath(m_nvg);
                for (int x = 0; x < W; x++)
                {
                    const float r2     = x * xscale;
                    const int   lo     = (int)r2;
                    const int   hi     = std::min(lo + 1, kAudioBins - 1);
                    const float frac   = r2 - lo;
                    const float val    = osc[lo] * (1.0f - frac) + osc[hi] * frac;
                    const float offset = (val - 128.0f) / 128.0f * h2;
                    const float y      = yBase + offset;
                    if (x == 0) nvgMoveTo(m_nvg, 0.5f, y);
                    else        nvgLineTo(m_nvg, x + 0.5f, y);
                }
                nvgStroke(m_nvg);
            }
            else
            {
                // Solid scope: filled polygon between waveform and baseline
                nvgBeginPath(m_nvg);
                nvgMoveTo(m_nvg, 0.0f, yBase);
                for (int x = 0; x < W; x++)
                {
                    const float r2     = x * xscale;
                    const int   lo     = (int)r2;
                    const int   hi     = std::min(lo + 1, kAudioBins - 1);
                    const float frac   = r2 - lo;
                    const float val    = osc[lo] * (1.0f - frac) + osc[hi] * frac;
                    const float offset = (val - 128.0f) / 128.0f * h2;
                    const float y      = yBase + offset;
                    nvgLineTo(m_nvg, (float)x, y);
                }
                nvgLineTo(m_nvg, (float)(W - 1), yBase);
                nvgClosePath(m_nvg);
                nvgFill(m_nvg);
            }
        }
    }

    nvgEndFrame(m_nvg);
    nvgluBindFramebuffer(nullptr);

    // ── View N+1 : composite pass — overlay onto InputTexture ─────────────────
    const uint8_t compView = Context.ViewId + 1;

    bgfx::setViewFrameBuffer(compView, Context.OutputFBO);
    bgfx::setViewRect(compView, 0, 0, (uint16_t)W, (uint16_t)H);
    bgfx::setViewClear(compView, BGFX_CLEAR_NONE);

    // Blend mode and alpha come from SetRenderMode (g_line_blend_mode).
    // Default (1u<<16): mode=0 Replace, alpha=0, lineWidth=1.
    const uint32_t lbmFull  = Context.LineBlendMode ? *Context.LineBlendMode : (1u << 16);
    const float    blendMode = (float)(lbmFull & 0xFF);
    const float    alpha     = (float)((lbmFull >> 8) & 0xFF);
    const float params[4] = { blendMode, alpha, 0.0f, 0.0f };
    bgfx::setUniform(m_paramsUniform, params);
    bgfx::setTexture(0, m_inputSampler,   Context.InputTexture);
    bgfx::setTexture(1, m_overlaySampler, bgfx::getTexture(m_overlayFbo->handle));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(compView, m_program);

    Context.FboManager->Swap();
}

void Simple::Destroy()
{
    DestroyOverlay();

    if (m_nvg)
    {
        nvgDelete(m_nvg);
        m_nvg = nullptr;
    }

    if (bgfx::isValid(m_paramsUniform))  bgfx::destroy(m_paramsUniform);
    if (bgfx::isValid(m_overlaySampler)) bgfx::destroy(m_overlaySampler);
    if (bgfx::isValid(m_inputSampler))   bgfx::destroy(m_inputSampler);
    if (bgfx::isValid(m_program))        bgfx::destroy(m_program);

    m_paramsUniform  = BGFX_INVALID_HANDLE;
    m_overlaySampler = BGFX_INVALID_HANDLE;
    m_inputSampler   = BGFX_INVALID_HANDLE;
    m_program        = BGFX_INVALID_HANDLE;
}
