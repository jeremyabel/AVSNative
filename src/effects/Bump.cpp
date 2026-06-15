#include "Bump.h"

#include "engine/JsonUtil.h"
#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_bump.sc.bin.h"

#include <algorithm>
#include <cmath>

static constexpr const char* NAME_Depth = "depth";
static constexpr const char* NAME_EnableOnBeatChange = "onBeat";
static constexpr const char* NAME_OnBeatDuration = "onBeatDuration";
static constexpr const char* NAME_OnBeatDepth = "onBeatDepth";
static constexpr const char* NAME_BlendMode = "blendMode";
static constexpr const char* NAME_ShowLightPos = "showLightPos";
static constexpr const char* NAME_InvertDepth = "invertDepth";

static const std::vector<std::string> LuaBuiltIns = { "x", "y", "bi", "isBeat", "isLBeat", "getspec", "getosc" };

void Bump::Init()
{
    bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_bump_spv, sizeof(fs_bump_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform  = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    TexelSizeUniform = bgfx::createUniform("u_texelSize", bgfx::UniformType::Vec4);
    Params1Uniform = bgfx::createUniform("u_bumpParams", bgfx::UniformType::Vec4);
    Params2Uniform = bgfx::createUniform("u_bumpFlags", bgfx::UniformType::Vec4);

    CurrentDepth = Depth;

    for (const auto& Variable : LuaBuiltIns)
    {
        LuaContext.SeedVar(Variable);
    }

    LuaContext.SetEnvNumber("bi", 1.0);
    LuaInitComplete = true;

    RecompileCode();
}

void Bump::Destroy()
{
    if (bgfx::isValid(Params2Uniform))  
        bgfx::destroy(Params2Uniform);

    if (bgfx::isValid(Params1Uniform)) 
        bgfx::destroy(Params1Uniform);

    if (bgfx::isValid(TexelSizeUniform))  
        bgfx::destroy(TexelSizeUniform);

    if (bgfx::isValid(TexUniform))  
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program))    
        bgfx::destroy(Program);

    Params2Uniform = BGFX_INVALID_HANDLE;
    Params1Uniform = BGFX_INVALID_HANDLE;
    TexelSizeUniform = BGFX_INVALID_HANDLE;
    TexUniform  = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
    LuaInitComplete = false;
}


void Bump::RecompileCode()
{
    if (!LuaInitComplete)
        return;

    LuaContext.CompileBlock(InitCode, NAME_InitCode, LuaRefInit);
    LuaContext.CompileBlock(FrameCode, NAME_FrameCode, LuaRefFrame);
    LuaContext.CompileBlock(BeatCode, NAME_BeatCode, LuaRefBeat);

    const std::string AllCode = InitCode + "\n" + FrameCode + "\n" + BeatCode;
    for (const auto& Variable : LuaRuntime::ScanVarDecls(AllCode, LuaBuiltIns))
    {
        LuaContext.SeedVar(Variable);
    }

    LuaContext.RunBlock(LuaRefInit, NAME_InitCode);
}

void Bump::Render(const RenderContext& Context)
{
    const int W = Context.Width;
    const int H = Context.Height;

    // Update beat flags (EEL convention: -1 = true, 1 = false).
    LuaContext.SetEnvNumber("isBeat", Context.IsBeat() ? -1.0 : 1.0);
    LuaContext.SetEnvNumber("isLBeat", OnBeatFadeout > 0 ? -1.0 : 1.0);

    LuaContext.RunBlock(LuaRefFrame, NAME_FrameCode);

    if (Context.IsBeat())
    {
        LuaContext.RunBlock(LuaRefBeat, NAME_BeatCode);
    }

    // Clamp bi to [0,1].
    const double BeatIntensity = std::clamp(LuaContext.GetEnvNumber("bi"), 0.0, 1.0);
    LuaContext.SetEnvNumber("bi", BeatIntensity);

    // On-beat depth snap (before bi multiplication, matching original).
    if (EnableOnBeatChange && Context.IsBeat())
    {
        CurrentDepth = OnBeatDepth;
        OnBeatFadeout = OnBeatDuration;
    }
    else if (!OnBeatFadeout)
    {
        CurrentDepth = Depth;
    }

    CurrentDepth = (int)((double)CurrentDepth * BeatIntensity);
    const int DepthScaled = (CurrentDepth * 256) / 100;

    // Light center: normalized [0,1] → pixel coords
    const int CenterX = LuaContext.GetEnvNumber("x") * W;
    const int CenterY = LuaContext.GetEnvNumber("y") * H;

    const float uTexelSize[4] = { 1.0f / (float)W, 1.0f / (float)H, (float)W, (float)H };
    const float uParams1[4] = { (float)CenterX, (float)CenterY, (float)DepthScaled, (float)BlendMode };
    const float uParams2[4] = { InvertDepth ? 1.0f : 0.0f, ShowLightPos ? 1.0f : 0.0f, 0.0f, 0.0f };

    bgfx::setUniform(TexelSizeUniform, uTexelSize);
    bgfx::setUniform(Params1Uniform, uParams1);
    bgfx::setUniform(Params2Uniform, uParams2);
    bgfx::setTexture(0, TexUniform, Context.InputTexture, BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();

    // Advance on-beat fadeout: step CurrentDepth toward base depth each frame.
    if (OnBeatFadeout > 0)
    {
        OnBeatFadeout--;
        if (OnBeatFadeout > 0 && OnBeatDuration > 0)
        {
            const int Step = std::abs(Depth - OnBeatDepth) / OnBeatDuration;
            CurrentDepth += Step * (OnBeatDepth > Depth ? -1 : 1);
        }
    }
}

nlohmann::json Bump::Serialize() const
{
    return 
    {
        { NAME_Depth, Depth },
        { NAME_EnableOnBeatChange, EnableOnBeatChange },
        { NAME_OnBeatDuration, OnBeatDuration },
        { NAME_OnBeatDepth, OnBeatDepth },
        { NAME_BlendMode, BlendMode },
        { NAME_ShowLightPos, ShowLightPos },
        { NAME_InvertDepth, InvertDepth },
        { NAME_InitCode, InitCode },
        { NAME_FrameCode, FrameCode },
        { NAME_BeatCode, BeatCode },
    };
}

void Bump::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Depth, Depth);
    JsonUtil::ReadBool(j, NAME_EnableOnBeatChange, EnableOnBeatChange);
    JsonUtil::ReadInt(j, NAME_OnBeatDuration, OnBeatDuration);
    JsonUtil::ReadInt(j, NAME_OnBeatDepth, OnBeatDepth);
    JsonUtil::ReadInt(j, NAME_BlendMode, BlendMode);
    JsonUtil::ReadBool(j, NAME_ShowLightPos, ShowLightPos);
    JsonUtil::ReadBool(j, NAME_InvertDepth, InvertDepth);
    JsonUtil::ReadString(j, NAME_InitCode, InitCode);
    JsonUtil::ReadString(j, NAME_FrameCode, FrameCode);
    JsonUtil::ReadString(j, NAME_BeatCode, BeatCode);

    RecompileCode();
}
