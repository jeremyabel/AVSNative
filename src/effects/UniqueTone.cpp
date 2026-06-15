#include "UniqueTone.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_uniquetone.sc.bin.h"

static constexpr const char* NAME_Color = "color";
static constexpr const char* NAME_Invert = "invert";
static constexpr const char* NAME_OutBlend = "outBlend";

void UniqueTone::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_uniquetone_spv, sizeof(fs_uniquetone_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_utColor", bgfx::UniformType::Vec4);
    ParamsUniform = bgfx::createUniform("u_utParams", bgfx::UniformType::Vec4);
}

void UniqueTone::Render(const RenderContext& Context)
{
    const float uToneColor[4] = { Color[0] / 255.f, Color[1] / 255.f, Color[2] / 255.f, 0.f };
    const float uParams[4] = { EnableInvert ? 1.f : 0.f, (float)OutBlend, 0.f, 0.f };

    bgfx::setUniform(ColorUniform, uToneColor);
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void UniqueTone::Destroy()
{
    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);
    
    if (bgfx::isValid(ColorUniform))
        bgfx::destroy(ColorUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json UniqueTone::Serialize() const
{
    return 
    {
        { NAME_Color, JsonUtil::ColorToJson(Color) },
        { NAME_Invert, EnableInvert },
        { NAME_OutBlend, OutBlend },
    };
}

void UniqueTone::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColor(j, NAME_Color, Color);
    JsonUtil::ReadBool(j, NAME_Invert, EnableInvert);
    JsonUtil::ReadInt(j, NAME_OutBlend, OutBlend);
}
