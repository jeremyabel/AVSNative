#include "ColorClip.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_colorclip.sc.bin.h"

void ColorClip::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_colorclip_spv, sizeof(fs_colorclip_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);
    
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_clipColor", bgfx::UniformType::Vec4);
}

void ColorClip::Render(const RenderContext& Context)
{
    const float Clip[4] = { Color[0] / 255.f, Color[1] / 255.f, Color[2] / 255.f, 0.f };
    
    bgfx::setUniform(ColorUniform, Clip);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void ColorClip::Destroy()
{
    if (bgfx::isValid(ColorUniform))
        bgfx::destroy(ColorUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json ColorClip::Serialize() const
{
    return {
        { kColor, JsonUtil::ColorToJson(Color) },
    };
}

void ColorClip::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColor(j, kColor, Color);
}
