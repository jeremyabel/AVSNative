#include "Clear.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_clear.sc.bin.h"

static constexpr const char* NAME_Color = "color";

void Clear::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_clear_spv, sizeof(fs_clear_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    ColorUniform = bgfx::createUniform("u_clearColor", bgfx::UniformType::Vec4);
}

void Clear::Destroy()
{
    if (bgfx::isValid(ColorUniform))
        bgfx::destroy(ColorUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ColorUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void Clear::Render(const RenderContext& Context)
{
    const float uClearColor[4] = { Color[0] / 255.f, Color[1] / 255.f, Color[2] / 255.f, 1.f };

    bgfx::setViewFrameBuffer(Context.ViewId, Context.FboManager->GetNext().Fbo);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)Context.Width, (uint16_t)Context.Height);
    bgfx::setUniform(ColorUniform, uClearColor);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json Clear::Serialize() const
{
    return 
    {
        { NAME_Color, JsonUtil::ColorToJson(Color) },
    };
}

void Clear::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColor(j, NAME_Color, Color);
}
