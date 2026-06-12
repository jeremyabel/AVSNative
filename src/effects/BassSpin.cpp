#include "BassSpin.h"
#include "engine/MathConstants.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <nanovg/nanovg.h>
#include <nanovg/nanovg_bgfx.h>

#include <algorithm>
#include <cmath>


void BassSpin::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_simple_spv,     sizeof(fs_simple_spv)));
    m_program = bgfx::createProgram(VS, FS, true);

    m_inputSampler   = bgfx::createUniform("s_input",        bgfx::UniformType::Sampler);
    m_overlaySampler = bgfx::createUniform("s_overlay",      bgfx::UniformType::Sampler);
    m_paramsUniform  = bgfx::createUniform("u_simpleParams",  bgfx::UniformType::Vec4);

    m_nvg = nvgCreate(0, 0);  // no edge AA — hard-edged fills match original
}

void BassSpin::EnsureOverlay(int W, int H)
{
    if (m_overlayW == W && m_overlayH == H)
        return;
    DestroyOverlay();
    m_overlayFbo = nvgluCreateFramebuffer(m_nvg, W, H, 0);
    m_overlayW   = W;
    m_overlayH   = H;
}

void BassSpin::DestroyOverlay()
{
    if (m_overlayFbo)
    {
        nvgluDeleteFramebuffer(m_overlayFbo);
        m_overlayFbo = nullptr;
    }
    m_overlayW = m_overlayH = 0;
}

void BassSpin::Render(const RenderContext& Context)
{
    const int W = Context.Width;
    const int H = Context.Height;

    EnsureOverlay(W, H);
    if (!m_overlayFbo)
        return;

    // Screen-space geometry (integer truncation matches JS Math.trunc)
    const int screenSize = std::min(H / 2, (W * 3) / 8);
    const int cy         = H / 2;

    // ── NanoVG overlay pass (ViewId) ─────────────────────────────────────────
    nvgluSetViewFramebuffer(Context.ViewId, m_overlayFbo);
    bgfx::setViewClear(Context.ViewId, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)W, (uint16_t)H);

    nvgluBindFramebuffer(m_overlayFbo);
    nvgBeginFrame(m_nvg, (float)W, (float)H, 1.0f);

    if (Context.AudioData)
    {
        const VisData& vis = *Context.AudioData;

        for (int tri = 0; tri < 2; tri++)
        {
            // tri=0: left-of-center, CW,  spec L, colorLeft,  gated by EnabledRight
            // tri=1: right-of-center, CCW, spec R, colorRight, gated by EnabledLeft
            const bool enabled = (tri == 0) ? EnabledRight : EnabledLeft;
            if (!enabled)
                continue;

            const float* specData = vis.spec[tri];
            const std::array<uint8_t, 3>& col = (tri == 0) ? ColorLeft : ColorRight;
            const float cx        = float((tri == 0) ? (W / 2 - screenSize / 2)
                                                      : (W / 2 + screenSize / 2));

            // Sum first 44 spectrum bins for bass energy
            float d = 0.0f;
            for (int x = 0; x < 44; x++)
                d += specData[x];

            // Normalize against smoothed previous value (shared across triangles, matches C++)
            int a = int((d * 512.0f) / (LastA + 30.0f * 256.0f));
            LastA = d;
            if (a > 255) a = 255;

            // Exponential velocity smoothing
            V[tri] = 0.7f * float(std::max(a - 104, 12)) / 96.0f + 0.3f * V[tri];
            Rv[tri] += avs::Pi / 6.0f * V[tri] * Dir[tri];

            // Arm endpoint (truncated to integer pixels, matching Math.trunc)
            const float sizeF = float(screenSize) * float(a) / 256.0f;
            const float xp    = std::trunc(std::cos(Rv[tri]) * sizeF);
            const float yp    = std::trunc(std::sin(Rv[tri]) * sizeF);

            const float px0 = cx + xp, py0 = float(cy) + yp;
            const float px1 = cx - xp, py1 = float(cy) - yp;

            const NVGcolor nvgCol = nvgRGBf(col[0] / 255.0f, col[1] / 255.0f, col[2] / 255.0f);

            if (Mode == 0)
            {
                // Outline: two spokes from center + trailing arc between frames
                nvgStrokeColor(m_nvg, nvgCol);
                nvgStrokeWidth(m_nvg, 1.0f);

                if (Lx[0][tri] != 0.0f || Ly[0][tri] != 0.0f)
                {
                    nvgBeginPath(m_nvg);
                    nvgMoveTo(m_nvg, Lx[0][tri], Ly[0][tri]);
                    nvgLineTo(m_nvg, px0, py0);
                    nvgStroke(m_nvg);
                }
                Lx[0][tri] = px0; Ly[0][tri] = py0;
                nvgBeginPath(m_nvg);
                nvgMoveTo(m_nvg, cx, float(cy));
                nvgLineTo(m_nvg, px0, py0);
                nvgStroke(m_nvg);

                if (Lx[1][tri] != 0.0f || Ly[1][tri] != 0.0f)
                {
                    nvgBeginPath(m_nvg);
                    nvgMoveTo(m_nvg, Lx[1][tri], Ly[1][tri]);
                    nvgLineTo(m_nvg, px1, py1);
                    nvgStroke(m_nvg);
                }
                Lx[1][tri] = px1; Ly[1][tri] = py1;
                nvgBeginPath(m_nvg);
                nvgMoveTo(m_nvg, cx, float(cy));
                nvgLineTo(m_nvg, px1, py1);
                nvgStroke(m_nvg);
            }
            else
            {
                // Filled: sweep triangle (center, prevTip, currentTip) each frame
                nvgFillColor(m_nvg, nvgCol);

                if (Lx[0][tri] != 0.0f || Ly[0][tri] != 0.0f)
                {
                    nvgBeginPath(m_nvg);
                    nvgMoveTo(m_nvg, cx, float(cy));
                    nvgLineTo(m_nvg, Lx[0][tri], Ly[0][tri]);
                    nvgLineTo(m_nvg, px0, py0);
                    nvgClosePath(m_nvg);
                    nvgFill(m_nvg);
                }
                Lx[0][tri] = px0; Ly[0][tri] = py0;

                if (Lx[1][tri] != 0.0f || Ly[1][tri] != 0.0f)
                {
                    nvgBeginPath(m_nvg);
                    nvgMoveTo(m_nvg, cx, float(cy));
                    nvgLineTo(m_nvg, Lx[1][tri], Ly[1][tri]);
                    nvgLineTo(m_nvg, px1, py1);
                    nvgClosePath(m_nvg);
                    nvgFill(m_nvg);
                }
                Lx[1][tri] = px1; Ly[1][tri] = py1;
            }
        }
    }

    nvgEndFrame(m_nvg);
    nvgluBindFramebuffer(nullptr);

    // ── Composite pass (ViewId+1): overlay onto InputTexture in Replace mode ──
    const uint8_t compView = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(compView, Context.OutputFBO);
    bgfx::setViewRect(compView, 0, 0, (uint16_t)W, (uint16_t)H);
    bgfx::setViewClear(compView, BGFX_CLEAR_NONE);

    const float params[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // mode=0 Replace, alpha=0
    bgfx::setUniform(m_paramsUniform, params);
    bgfx::setTexture(0, m_inputSampler,   Context.InputTexture);
    bgfx::setTexture(1, m_overlaySampler, bgfx::getTexture(m_overlayFbo->handle));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(compView, m_program);

    Context.FboManager->Swap();
}

void BassSpin::Destroy()
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

nlohmann::json BassSpin::Serialize() const
{
    return {
        { kEnabledLeft,  EnabledLeft  },
        { kEnabledRight, EnabledRight },
        { kColorLeft,    JsonUtil::ColorToJson(ColorLeft)  },
        { kColorRight,   JsonUtil::ColorToJson(ColorRight) },
        { kMode,         Mode },
    };
}

void BassSpin::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadBool (j, kEnabledLeft,  EnabledLeft);
    JsonUtil::ReadBool (j, kEnabledRight, EnabledRight);
    JsonUtil::ReadColor(j, kColorLeft,    ColorLeft);
    JsonUtil::ReadColor(j, kColorRight,   ColorRight);
    JsonUtil::ReadInt  (j, kMode,         Mode);
}
