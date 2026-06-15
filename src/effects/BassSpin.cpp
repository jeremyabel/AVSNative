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

static constexpr const char* NAME_EnabledLeft  = "enabledLeft";
static constexpr const char* NAME_EnabledRight = "enabledRight";
static constexpr const char* NAME_ColorLeft = "colorLeft";
static constexpr const char* NAME_ColorRight = "colorRight";
static constexpr const char* NAME_Mode = "mode";

void BassSpin::Init()
{
    bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_simple_spv, sizeof(fs_simple_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    OverlayUniform = bgfx::createUniform("s_overlay", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_simpleParams",  bgfx::UniformType::Vec4);

    NvgContext = nvgCreate(0, 0);  // no edge AA — hard-edged fills match original
}

void BassSpin::EnsureOverlay(int W, int H)
{
    if (OverlayW == W && OverlayH == H)
        return;

    DestroyOverlay();
    OverlayFBO = nvgluCreateFramebuffer(NvgContext, W, H, 0);
    OverlayW = W;
    OverlayH = H;
}

void BassSpin::DestroyOverlay()
{
    if (OverlayFBO)
    {
        nvgluDeleteFramebuffer(OverlayFBO);
        OverlayFBO = nullptr;
    }
    OverlayW = OverlayH = 0;
}

void BassSpin::Render(const RenderContext& Context)
{
    const int W = Context.Width;
    const int H = Context.Height;

    EnsureOverlay(W, H);
    if (!OverlayFBO)
        return;

    // Screen-space geometry (integer truncation matches JS Math.trunc)
    const int screenSize = std::min(H / 2, (W * 3) / 8);
    const int cy = H / 2;

    // NanoVG overlay pass (ViewId)
    nvgluSetViewFramebuffer(Context.ViewId, OverlayFBO);
    bgfx::setViewClear(Context.ViewId, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)W, (uint16_t)H);

    nvgluBindFramebuffer(OverlayFBO);
    nvgBeginFrame(NvgContext, (float)W, (float)H, 1.0f);

    if (Context.AudioData)
    {
        const VisData& AudioData = *Context.AudioData;

        for (int tri = 0; tri < 2; tri++)
        {
            // tri=0: left-of-center, CW,  spec L, colorLeft,  gated by EnabledRight
            // tri=1: right-of-center, CCW, spec R, colorRight, gated by EnabledLeft
            const bool enabled = (tri == 0) ? EnabledRight : EnabledLeft;
            if (!enabled)
                continue;

            const float* specData = AudioData.spec[tri];
            const std::array<uint8_t, 3>& col = (tri == 0) ? ColorLeft : ColorRight;
            const float cx = float((tri == 0) ? (W / 2 - screenSize / 2) : (W / 2 + screenSize / 2));

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
            const float xp = std::trunc(std::cos(Rv[tri]) * sizeF);
            const float yp = std::trunc(std::sin(Rv[tri]) * sizeF);

            const float px0 = cx + xp, py0 = float(cy) + yp;
            const float px1 = cx - xp, py1 = float(cy) - yp;

            const NVGcolor nvgCol = nvgRGBf(col[0] / 255.0f, col[1] / 255.0f, col[2] / 255.0f);

            if (Mode == 0)
            {
                // Outline: two spokes from center + trailing arc between frames
                nvgStrokeColor(NvgContext, nvgCol);
                nvgStrokeWidth(NvgContext, 1.0f);

                if (Lx[0][tri] != 0.0f || Ly[0][tri] != 0.0f)
                {
                    nvgBeginPath(NvgContext);
                    nvgMoveTo(NvgContext, Lx[0][tri], Ly[0][tri]);
                    nvgLineTo(NvgContext, px0, py0);
                    nvgStroke(NvgContext);
                }
                Lx[0][tri] = px0; Ly[0][tri] = py0;
                nvgBeginPath(NvgContext);
                nvgMoveTo(NvgContext, cx, float(cy));
                nvgLineTo(NvgContext, px0, py0);
                nvgStroke(NvgContext);

                if (Lx[1][tri] != 0.0f || Ly[1][tri] != 0.0f)
                {
                    nvgBeginPath(NvgContext);
                    nvgMoveTo(NvgContext, Lx[1][tri], Ly[1][tri]);
                    nvgLineTo(NvgContext, px1, py1);
                    nvgStroke(NvgContext);
                }
                Lx[1][tri] = px1; Ly[1][tri] = py1;
                nvgBeginPath(NvgContext);
                nvgMoveTo(NvgContext, cx, float(cy));
                nvgLineTo(NvgContext, px1, py1);
                nvgStroke(NvgContext);
            }
            else
            {
                // Filled: sweep triangle (center, prevTip, currentTip) each frame
                nvgFillColor(NvgContext, nvgCol);

                if (Lx[0][tri] != 0.0f || Ly[0][tri] != 0.0f)
                {
                    nvgBeginPath(NvgContext);
                    nvgMoveTo(NvgContext, cx, float(cy));
                    nvgLineTo(NvgContext, Lx[0][tri], Ly[0][tri]);
                    nvgLineTo(NvgContext, px0, py0);
                    nvgClosePath(NvgContext);
                    nvgFill(NvgContext);
                }
                Lx[0][tri] = px0; Ly[0][tri] = py0;

                if (Lx[1][tri] != 0.0f || Ly[1][tri] != 0.0f)
                {
                    nvgBeginPath(NvgContext);
                    nvgMoveTo(NvgContext, cx, float(cy));
                    nvgLineTo(NvgContext, Lx[1][tri], Ly[1][tri]);
                    nvgLineTo(NvgContext, px1, py1);
                    nvgClosePath(NvgContext);
                    nvgFill(NvgContext);
                }
                Lx[1][tri] = px1; Ly[1][tri] = py1;
            }
        }
    }

    nvgEndFrame(NvgContext);
    nvgluBindFramebuffer(nullptr);

    // Composite pass (ViewId+1): overlay onto InputTexture in Replace mode
    const uint8_t CompViewId = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(CompViewId, Context.OutputFBO);
    bgfx::setViewRect(CompViewId, 0, 0, (uint16_t)W, (uint16_t)H);
    bgfx::setViewClear(CompViewId, BGFX_CLEAR_NONE);

    const float uParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // mode=0 Replace, alpha=0
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform,   Context.InputTexture);
    bgfx::setTexture(1, OverlayUniform, bgfx::getTexture(OverlayFBO->handle));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(CompViewId, Program);

    Context.FboManager->Swap();
}

void BassSpin::Destroy()
{
    DestroyOverlay();

    if (NvgContext)
    {
        nvgDelete(NvgContext);
        NvgContext = nullptr;
    }

    if (bgfx::isValid(ParamsUniform))  
        bgfx::destroy(ParamsUniform);

    if (bgfx::isValid(OverlayUniform)) 
        bgfx::destroy(OverlayUniform);

    if (bgfx::isValid(TexUniform))   
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program))        
        bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    OverlayUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json BassSpin::Serialize() const
{
    return 
    {
        { NAME_EnabledLeft, EnabledLeft },
        { NAME_EnabledRight, EnabledRight },
        { NAME_ColorLeft, JsonUtil::ColorToJson(ColorLeft) },
        { NAME_ColorRight, JsonUtil::ColorToJson(ColorRight) },
        { NAME_Mode, Mode },
    };
}

void BassSpin::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadBool(j, NAME_EnabledLeft, EnabledLeft);
    JsonUtil::ReadBool(j, NAME_EnabledRight, EnabledRight);
    JsonUtil::ReadColor(j, NAME_ColorLeft, ColorLeft);
    JsonUtil::ReadColor(j, NAME_ColorRight, ColorRight);
    JsonUtil::ReadInt(j, NAME_Mode, Mode);
}
