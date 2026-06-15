#include "FadeOut.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_fadeout.sc.bin.h"

static constexpr const char* NAME_Speed = "speed";
static constexpr const char* NAME_Color = "color";

void FadeOut::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_fadeout_spv, sizeof(fs_fadeout_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    ParamsUniform = bgfx::createUniform("u_fadeParams", bgfx::UniformType::Vec4);
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
}

void FadeOut::Destroy()
{
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    TexUniform = BGFX_INVALID_HANDLE;
    ParamsUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void FadeOut::Render(const RenderContext& Context)
{
    // Speed maps directly to blend amount: 0 = no fade, 1 = instant wipe to color.
    // speed=0.08 → 8% per frame toward target, equivalent to AVS_Remake fade=0.92.
    const float uParams[4] = { Color[0] / 255.f, Color[1] / 255.f, Color[2] / 255.f, Speed };

    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json FadeOut::Serialize() const
{
    return 
    {
        { NAME_Speed, Speed },
        { NAME_Color, JsonUtil::ColorToJson(Color) },
    };
}

void FadeOut::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadFloat(j, NAME_Speed, Speed);
    JsonUtil::ReadColor(j, NAME_Color, Color);
}
