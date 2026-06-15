#include "DynamicShift.h"

#include "engine/JsonUtil.h"
#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_dynamicshift.sc.bin.h"

#include <algorithm>

static constexpr const char* NAME_EnableBlend = "blend";
static constexpr const char* NAME_Bilinear = "subpixel";
static constexpr const char* NAME_BilinearCompat = "bilinearCompat";

static const std::vector<std::string> LuaBuiltIns = { "x", "y", "alpha", "w", "h", "b", "getspec", "getosc" };

void DynamicShift::RescanUserVars()
{
    auto vars = LuaRuntime::ScanVarDecls(InitCode, LuaBuiltIns);
    for (const auto& Variable : vars)
    {
        LuaContext.SeedVar(Variable);
    }
}

void DynamicShift::Init()
{
    bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_dynamicshift_spv, sizeof(fs_dynamicshift_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    Params1Uniform = bgfx::createUniform("u_ds_params0", bgfx::UniformType::Vec4);
    Params2Uniform = bgfx::createUniform("u_ds_params1", bgfx::UniformType::Vec4);
    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);

    // Seed all built-ins so arithmetic on them never errors.
    for (const auto& Variable : LuaBuiltIns)
    {
        LuaContext.SeedVar(Variable);
    }

    LuaContext.SetEnvNumber("x", 0.0);
    LuaContext.SetEnvNumber("y", 0.0);
    LuaContext.SetEnvNumber("alpha", 0.5);

    RescanUserVars();

    LuaContext.CompileBlock(InitCode, NAME_InitCode, LuaRefInit);
    LuaContext.CompileBlock(FrameCode, NAME_FrameCode, LuaRefFrame);
    LuaContext.CompileBlock(BeatCode, NAME_BeatCode, m_beatRef);

    LuaContext.SetEnvNumber("b", 0.0);
    LuaContext.RunBlock(LuaRefInit, NAME_InitCode);
    m_inited = true;
}

void DynamicShift::Render(const RenderContext& Context)
{
    LuaContext.SetAudioData(Context.AudioData);
    LuaContext.SetEnvNumber("b", Context.IsBeat() ? 1.0 : 0.0);
    LuaContext.SetEnvNumber("w", (double)Context.Width);
    LuaContext.SetEnvNumber("h", (double)Context.Height);

    LuaContext.RunBlock(LuaRefFrame, NAME_FrameCode);

    if (Context.IsBeat())
    {
        LuaContext.RunBlock(m_beatRef, NAME_BeatCode);
    }

    const double X = LuaContext.GetEnvNumber("x");
    const double Y = LuaContext.GetEnvNumber("y");
    const double Alpha = std::clamp(LuaContext.GetEnvNumber("alpha"), 0.0, 1.0);

    // compat = original AVS 8-bit integer bilinear (only meaningful when Subpixel).
    const bool compat = Bilinear && Compat;

    const float uParams1[4] = { (float)X, (float)Y, (float)Context.Width, (float)Context.Height };
    const float uParams2[4] = { EnableBlend ? 1.0f : 0.0f, (float)Alpha, compat ? 1.0f : 0.0f, 0.0f };
    bgfx::setUniform(Params1Uniform, uParams1);
    bgfx::setUniform(Params2Uniform, uParams2);

    // Compat does its own integer texelFetch blend → bind POINT. Otherwise bilinear
    // when Subpixel, else nearest.
    uint32_t samplerFlags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    if (!Bilinear || compat)
        samplerFlags |= BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT;

    bgfx::setTexture(0, TexUniform, Context.InputTexture, samplerFlags);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void DynamicShift::RecompileInitCode()
{
    if (!m_inited)
        return;

    RescanUserVars();
    LuaContext.CompileBlock(InitCode, NAME_InitCode, LuaRefInit);
    LuaContext.SetEnvNumber("b", 0.0);
    LuaContext.RunBlock(LuaRefInit, NAME_InitCode);
}

void DynamicShift::RecompileFrameCode()
{
    if (!m_inited) 
        return;

    LuaContext.CompileBlock(FrameCode, NAME_FrameCode, LuaRefFrame);
}

void DynamicShift::RecompileBeatCode()
{
    if (!m_inited) 
        return;

    LuaContext.CompileBlock(BeatCode, NAME_BeatCode, m_beatRef);
}

void DynamicShift::Destroy()
{
    if (bgfx::isValid(Program))     
        bgfx::destroy(Program);

    if (bgfx::isValid(Params1Uniform)) 
        bgfx::destroy(Params1Uniform);

    if (bgfx::isValid(Params2Uniform)) 
        bgfx::destroy(Params2Uniform);

    if (bgfx::isValid(TexUniform))   
        bgfx::destroy(TexUniform);

    Program = BGFX_INVALID_HANDLE;
    Params1Uniform = BGFX_INVALID_HANDLE;
    Params2Uniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    m_inited = false;
}

nlohmann::json DynamicShift::Serialize() const
{
    return 
    {
        { NAME_InitCode, InitCode },
        { NAME_FrameCode, FrameCode },
        { NAME_BeatCode, BeatCode },
        { NAME_EnableBlend, EnableBlend },
        { NAME_Bilinear, Bilinear },
        { NAME_BilinearCompat, Compat },
    };
}

void DynamicShift::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadString(j, NAME_InitCode, InitCode);
    JsonUtil::ReadString(j, NAME_FrameCode, FrameCode);
    JsonUtil::ReadString(j, NAME_BeatCode, BeatCode);
    JsonUtil::ReadBool(j, NAME_EnableBlend, EnableBlend);
    JsonUtil::ReadBool(j, NAME_Bilinear, Bilinear);
    JsonUtil::ReadBool(j, NAME_BilinearCompat, Compat);

    RecompileInitCode();
    RecompileFrameCode();
    RecompileBeatCode();
}
