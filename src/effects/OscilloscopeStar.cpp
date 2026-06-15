#include "OscilloscopeStar.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <nanovg/nanovg.h>
#include <nanovg/nanovg_bgfx.h>

#include <algorithm>
#include <cmath>

#include "engine/MathConstants.h"

static constexpr float kDfactorStart = 1.0f / 1024.0f;
static constexpr float kDfactorStep = (kDfactorStart - 1.0f / 128.0f) / 64.0f;

// ── Init / Destroy ────────────────────────────────────────────────────────────

void OscilloscopeStar::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_simple_spv, sizeof(fs_simple_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    OverlayUniform = bgfx::createUniform("s_overlay", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_simpleParams",  bgfx::UniformType::Vec4);

    NvgContext = nvgCreate(1, 0);
}

void OscilloscopeStar::Destroy()
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

void OscilloscopeStar::EnsureOverlay(int W, int H)
{
    if (OverlayWidth == W && OverlayHeight == H) 
        return;

    DestroyOverlay();
    OverlayFBO = nvgluCreateFramebuffer(NvgContext, W, H, 0);
    OverlayWidth = W;
    OverlayHeight = H;
}

void OscilloscopeStar::DestroyOverlay()
{
    if (OverlayFBO) 
    { 
        nvgluDeleteFramebuffer(OverlayFBO); 
        OverlayFBO = nullptr; 
    }

    OverlayWidth = OverlayHeight = 0;
}

void OscilloscopeStar::Render(const RenderContext& Context)
{
    if (Colors.empty()) 
        return;

    const int W = Context.Width;
    const int H = Context.Height;

    EnsureOverlay(W, H);
    if (!OverlayFBO) 
        return;

    // Color cycling (64-step interpolation, shared helper)
    const auto [cr, cg, cb] = Colors.StepF();

    // Audio data — waveform (osc) only, always; center = signed average of L and R
    float faData[NumAudioBins] = {};
    if (Context.AudioData)
    {
        const VisData& vis = *Context.AudioData;
        if (AudioChannel == 2)
        {
            for (int i = 0; i < NumAudioBins; i++)
            {
                const int v0 = std::clamp((int)vis.osc[0][i], 0, 255);
                const int v1 = std::clamp((int)vis.osc[1][i], 0, 255);
                faData[i] = (float)(((v0 - 128) / 2 + (v1 - 128) / 2 + 128) & 0xff);
            }
        }
        else
        {
            for (int i = 0; i < NumAudioBins; i++)
                faData[i] = vis.osc[AudioChannel][i];
        }
    }
    else
    {
        for (int i = 0; i < NumAudioBins; i++) faData[i] = 128.0f;
    }

    // Screen geometry
    const float fsize = Size / 32.0f;
    const float sizePx = std::min(H * fsize, W * fsize);
    const float cy = (float)(H / 2);
    float cx;
    if      (Position == 0) cx = (float)(W / 4);
    else if (Position == 1) cx = (float)(W / 2 + W / 4);
    else                    cx = (float)(W / 2);

    const float dp = sizePx / 64.0f;

    // ── NanoVG overlay pass ───────────────────────────────────────────────────
    nvgluSetViewFramebuffer(Context.ViewId, OverlayFBO);
    bgfx::setViewClear(Context.ViewId, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)W, (uint16_t)H);

    nvgluBindFramebuffer(OverlayFBO);
    nvgBeginFrame(NvgContext, (float)W, (float)H, 1.0f);

    nvgStrokeColor(NvgContext, nvgRGBf(cr, cg, cb));
    nvgStrokeWidth(NvgContext, 1.0f);

    // 5 arms, each 64 segments of waveform displaced perpendicular to arm direction.
    // Audio index ii is sequential across all arms (0..319), sampling osc data.
    int ii = 0;
    for (int q = 0; q < 5; q++)
    {
        const float Angle = CurrentRotation + q * (avs::TwoPi / 5.0f);
        const float CosA = std::cos(Angle);
        const float SinA = std::sin(Angle);

        float dfactor = kDfactorStart;
        float p_dist  = 0.0f;

        // Each arm starts from the screen center (matches C++ `lx=c_x; ly=h/2`)
        float lx = cx, ly = cy;

        for (int s = 0; s < 64; s++, ii++)
        {
            const float ale = (faData[ii] - 128.0f) * dfactor * sizePx;
            const float nx  = std::trunc(cx + CosA * p_dist - SinA * ale);
            const float ny  = std::trunc(cy + SinA * p_dist + CosA * ale);

            const bool lOn = (lx >= 0.0f && lx < W && ly >= 0.0f && ly < H);
            const bool nOn = (nx >= 0.0f && nx < W && ny >= 0.0f && ny < H);
            if (lOn || nOn)
            {
                nvgBeginPath(NvgContext);
                nvgMoveTo(NvgContext, lx, ly);
                nvgLineTo(NvgContext, nx, ny);
                nvgStroke(NvgContext);
            }

            lx = nx; ly = ny;
            p_dist  += dp;
            dfactor += kDfactorStep;
        }
    }

    // Advance rotation, wrapping in [0, 2π)
    CurrentRotation += 0.01f * (float)Rotation;
    while (CurrentRotation >= avs::TwoPi) CurrentRotation -= avs::TwoPi;
    while (CurrentRotation <  0.0f)       CurrentRotation += avs::TwoPi;

    nvgEndFrame(NvgContext);
    nvgluBindFramebuffer(nullptr);

    // ── Composite pass ────────────────────────────────────────────────────────
    const uint8_t CompView = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(CompView, Context.OutputFBO);
    bgfx::setViewRect(CompView, 0, 0, (uint16_t)W, (uint16_t)H);
    bgfx::setViewClear(CompView, BGFX_CLEAR_NONE);

    const float uParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform,   Context.InputTexture);
    bgfx::setTexture(1, OverlayUniform, bgfx::getTexture(OverlayFBO->handle));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(CompView, Program);

    Context.FboManager->Swap();
}

nlohmann::json OscilloscopeStar::Serialize() const
{
    return 
    {
        { NAME_Colors, JsonUtil::ColorsToJson(Colors) },
        { NAME_AudioChannel, AudioChannel },
        { NAME_Position, Position },
        { NAME_Size, Size },
        { NAME_Rotation, Rotation },
    };
}

void OscilloscopeStar::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColors(j, NAME_Colors, Colors);
    JsonUtil::ReadInt(j, NAME_AudioChannel, AudioChannel);
    JsonUtil::ReadInt(j, NAME_Position, Position);
    JsonUtil::ReadInt(j, NAME_Size, Size);
    JsonUtil::ReadInt(j, NAME_Rotation, Rotation);
}
