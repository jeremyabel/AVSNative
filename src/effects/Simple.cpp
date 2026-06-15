#include "Simple.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <nanovg/nanovg.h>
#include <nanovg/nanovg_bgfx.h>

#include <algorithm>

static constexpr const char* NAME_Mode = "mode";
static constexpr const char* NAME_Channel = "channel";
static constexpr const char* NAME_Position = "position";
static constexpr const char* NAME_Colors = "colors";
static constexpr const char* NAME_Antialiasing = "antialiasing";

void Simple::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_simple_spv, sizeof(fs_simple_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    OverlayUniform = bgfx::createUniform("s_overlay", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_simpleParams", bgfx::UniformType::Vec4);

    EnsureNvgContext();
}

void Simple::Destroy()
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


void Simple::EnsureNvgContext()
{
    if (NvgContext && NvgEdgeAA == AntialiasingEnabled)
        return;

    // EdgeAA is baked into the context at creation time, so recreate when it changes.
    DestroyOverlay();
    if (NvgContext)
    {
        nvgDelete(NvgContext);
    }

    NvgEdgeAA = AntialiasingEnabled;
    NvgContext = nvgCreate(AntialiasingEnabled ? 1 : 0, 0);
}

void Simple::EnsureOverlay(int Width, int Height)
{
    if (OverlayW == Width && OverlayH == Height)
        return;

    DestroyOverlay();
    OverlayFBO = nvgluCreateFramebuffer(NvgContext, Width, Height, 0);
    OverlayW = Width;
    OverlayH = Height;
}

void Simple::DestroyOverlay()
{
    if (OverlayFBO)
    {
        nvgluDeleteFramebuffer(OverlayFBO);
        OverlayFBO = nullptr;
    }

    OverlayW = 0;
    OverlayH = 0;
}

void Simple::Render(const RenderContext& Context)
{
    const int W = Context.Width;
    const int H = Context.Height;
    const int HalfH = H / 2;

    EnsureNvgContext();
    EnsureOverlay(W, H);
    if (!OverlayFBO)
        return;

    // View N: NanoVG overlay pass
    const uint8_t NvgView = Context.ViewId;

    // Point view at overlay, set sequential mode
    nvgluSetViewFramebuffer(NvgView, OverlayFBO);  
    bgfx::setViewClear(NvgView, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(NvgView, 0, 0, (uint16_t)W, (uint16_t)H);

    nvgluBindFramebuffer(OverlayFBO);
    nvgBeginFrame(NvgContext, (float)W, (float)H, 1.0f);

    if (Context.AudioData)
    {
        const VisData& AudioData = *Context.AudioData;

        // Get audio data
        float SpecData[NumAudioBins], OscData[NumAudioBins];
        if (Channel == 2)
        {
            for (int i = 0; i < NumAudioBins; i++)
            {
                // Mono mix
                SpecData[i] = (AudioData.spec[0][i] + AudioData.spec[1][i]) * 0.5f;
                OscData[i] = (AudioData.osc[0][i] + AudioData.osc[1][i]) * 0.5f;
            }
        }
        else
        {
            for (int i = 0; i < NumAudioBins; i++)
            {
                SpecData[i] = AudioData.spec[Channel][i];
                OscData[i] = AudioData.osc[Channel][i];
            }
        }

        float YBaseline = 0;
        if (Position == 0)
            YBaseline = 0.f;
        else if (Position == 2) 
            YBaseline = (float)(H - 1);
        else                    
            YBaseline = (float)HalfH;

        const float LineWidth = std::max(1.0f, (float)(Context.LineMode ? Context.LineMode->Width : 1));

        const auto [cr, cg, cb] = Colors.StepF();
        nvgStrokeColor(NvgContext, nvgRGBAf(cr, cg, cb, 1.0f));
        nvgFillColor(NvgContext, nvgRGBAf(cr, cg, cb, 1.0f));
        nvgStrokeWidth(NvgContext, LineWidth);

        const float XPerStep = (float)NumAudioBins / W;

        if (Mode == 0 || Mode == 1)
        {
            auto GetSpectrumPoint = [&](int XIndex) -> float
            {
                const float XNorm = XIndex * XPerStep;
                const int CurrX = (int)XNorm;
                const int NextX = std::min(CurrX + 1, NumAudioBins - 1);
                const float XFrac = XNorm - CurrX;
                const float InterpValue = SpecData[CurrX] * (1.0f - XFrac) + SpecData[NextX] * XFrac;
                return YBaseline - InterpValue / 255.0f * HalfH;
            };

            // Analyzer modes: spectrum data
            if (Mode == 0)
            {
                // Solid analyzer: filled polygon from baseline through spectrum top
                nvgBeginPath(NvgContext);
                nvgMoveTo(NvgContext, 0.0f, YBaseline);
                for (int X = 0; X < W; X++)
                {
                    nvgLineTo(NvgContext, (float)X, GetSpectrumPoint(X));
                }
                nvgLineTo(NvgContext, (float)(W - 1), YBaseline);
                nvgClosePath(NvgContext);
                nvgFill(NvgContext);
            }
            else
            {
                // Line analyzer: connected polyline along spectrum top
                nvgBeginPath(NvgContext);
                for (int X = 0; X < W; X++)
                {
                    const float Y = GetSpectrumPoint(X);
                    if (X == 0) nvgMoveTo(NvgContext, 0.5f, Y);
                    else        nvgLineTo(NvgContext, X + 0.5f, Y);
                }
                nvgStroke(NvgContext);
            }
        }
        else
        {
            auto GetOscPoint = [&](int XIndex) -> float
            {
                const float XNorm = XIndex * XPerStep;
                const int CurrX = (int)XNorm;
                const int NextX = std::min(CurrX + 1, NumAudioBins - 1);
                const float XFrac = XNorm - CurrX;
                const float InterpValue = OscData[CurrX] * (1.0f - XFrac) + OscData[NextX] * XFrac;
                const float Offset = (InterpValue - 128.f) / 128. * HalfH;
                return YBaseline + Offset;
            };

            // Scope modes: waveform data (128 = zero-crossing)
            if (Mode == 2)
            {
                // Line scope
                nvgBeginPath(NvgContext);
                for (int X = 0; X < W; X++)
                {
                    const float Y = GetOscPoint(X);
                    if (X == 0) nvgMoveTo(NvgContext, 0.5f, Y);
                    else        nvgLineTo(NvgContext, X + 0.5f, Y);
                }
                nvgStroke(NvgContext);
            }
            else
            {
                // Solid scope
                nvgBeginPath(NvgContext);
                nvgMoveTo(NvgContext, 0.0f, YBaseline);
                for (int X = 0; X < W; X++)
                {
                    const float Y = GetOscPoint(X);
                    nvgLineTo(NvgContext, (float)X, Y);
                }
                nvgLineTo(NvgContext, (float)(W - 1), YBaseline);
                nvgClosePath(NvgContext);
                nvgFill(NvgContext);
            }
        }
    }

    nvgEndFrame(NvgContext);
    nvgluBindFramebuffer(nullptr);

    // View N+1: composite pass, overlay onto InputTexture
    const uint8_t CompViewId = Context.ViewId + 1;

    bgfx::setViewFrameBuffer(CompViewId, Context.OutputFBO);
    bgfx::setViewRect(CompViewId, 0, 0, (uint16_t)W, (uint16_t)H);
    bgfx::setViewClear(CompViewId, BGFX_CLEAR_NONE);

    // Blend mode and alpha come from SetRenderMode. Default: Replace, alpha 0.
    const float BlendMode = Context.LineMode ? (float)Context.LineMode->Blend : 0.0f;
    const float Alpha = Context.LineMode ? (float)Context.LineMode->Alpha : 0.0f;
    const float uParams[4] = { BlendMode, Alpha, 0.0f, 0.0f };
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setTexture(1, OverlayUniform, bgfx::getTexture(OverlayFBO->handle));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(CompViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json Simple::Serialize() const
{
    return 
    {
        { NAME_Mode, Mode },
        { NAME_Channel, Channel },
        { NAME_Position, Position },
        { NAME_Colors, JsonUtil::ColorsToJson(Colors) },
        { NAME_Antialiasing, AntialiasingEnabled },
    };
}

void Simple::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Mode, Mode);
    JsonUtil::ReadInt(j, NAME_Channel, Channel);
    JsonUtil::ReadInt(j, NAME_Position, Position);
    JsonUtil::ReadColors(j, NAME_Colors, Colors);
    JsonUtil::ReadBool(j, NAME_Antialiasing, AntialiasingEnabled);
}
