#include "Grain.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_grain.sc.bin.h"

static constexpr const char* NAME_Amount = "amount";
static constexpr const char* NAME_BlendMode = "blendMode";
static constexpr const char* NAME_IsStatic = "isStatic";

void Grain::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_grain_spv, sizeof(fs_grain_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_grainParams", bgfx::UniformType::Vec4);
}

void Grain::Destroy()
{
    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void Grain::Render(const RenderContext& Context)
{
    const float uParams[4] = { (float)Amount, (float)BlendMode, IsStatic ? 1.f : 0.f, (float)(Context.Frame & 0xFFFF) };

    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json Grain::Serialize() const
{
    return 
    {
        { NAME_Amount, Amount },
        { NAME_BlendMode, BlendMode },
        { NAME_IsStatic,  IsStatic  },
    };
}

void Grain::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Amount, Amount);
    JsonUtil::ReadInt(j, NAME_BlendMode, BlendMode);
    JsonUtil::ReadBool(j, NAME_IsStatic, IsStatic);
}
