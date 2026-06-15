#include "effects/Ramp.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include <bgfx/bgfx.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_ramp.sc.bin.h"

static constexpr const char* NAME_Type = "type";
static constexpr const char* NAME_Scale = "scale";
static constexpr const char* NAME_Rotation = "rotation";
static constexpr const char* kOffset = "offset";
static constexpr const char* NAME_BlendMode = "blendmode";
static constexpr const char* NAME_UseWindowAspect = "useWindowAspect";

void Ramp::Init()
{
    bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_ramp_spv, sizeof(fs_ramp_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    Params1Uniform = bgfx::createUniform("u_rampParams", bgfx::UniformType::Vec4);
    Params2Uniform = bgfx::createUniform("u_rampParams2", bgfx::UniformType::Vec4);
}

void Ramp::Render(const RenderContext& Context)
{
    const float AspectRatio = (UseWindowAspect && Context.Height > 0) ? (float)Context.Width / (float)Context.Height : 1.0f;

    float uParams1[4] = { (float)Type, Scale, Rotation * 3.14159265358979f / 180.0f, (float)BlendMode };
    float uParams2[4] = { AspectRatio, Offset, 0.0f, 0.0f };

    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setUniform(Params1Uniform, uParams1);
    bgfx::setUniform(Params2Uniform, uParams2);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Ramp::Destroy()
{
    if (bgfx::isValid(Params2Uniform)) 
        bgfx::destroy(Params2Uniform);

    if (bgfx::isValid(Params1Uniform))  
        bgfx::destroy(Params1Uniform);

    if (bgfx::isValid(TexUniform))   
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program))        
        bgfx::destroy(Program);

    Params2Uniform = BGFX_INVALID_HANDLE;
    Params1Uniform  = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json Ramp::Serialize() const
{
    return 
    {
        { NAME_Type, Type },
        { NAME_Scale, Scale },
        { NAME_Rotation, Rotation },
        { kOffset, Offset },
        { NAME_BlendMode, BlendMode },
        { NAME_UseWindowAspect, UseWindowAspect },
    };
}

void Ramp::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Type, Type);
    JsonUtil::ReadFloat(j, NAME_Scale, Scale);
    JsonUtil::ReadFloat(j, NAME_Rotation, Rotation);
    JsonUtil::ReadFloat(j, kOffset, Offset);
    JsonUtil::ReadInt(j, NAME_BlendMode, BlendMode);
    JsonUtil::ReadBool(j, NAME_UseWindowAspect, UseWindowAspect);
}
