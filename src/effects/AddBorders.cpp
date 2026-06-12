#include "AddBorders.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_addborders.sc.bin.h"

#include <algorithm>
#include <cmath>

void AddBorders::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_addborders_spv, sizeof(fs_addborders_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    BorderParamsUniform = bgfx::createUniform("u_borderParams", bgfx::UniformType::Vec4);
    BorderColorUniform  = bgfx::createUniform("u_borderColor", bgfx::UniformType::Vec4);
}

void AddBorders::Render(const RenderContext& Context)
{
    const float W = (float)Context.Width;
    const float H = (float)Context.Height;

    const float BorderW = std::max(1.0f, std::floor(W * Size / 100.f));
    const float BorderH = std::max(1.0f, std::floor(H * Size / 100.f));

    const float Params[4] = { BorderW, BorderH, W, H };
    const float BorderColor[4] = { Color[0] / 255.f, Color[1] / 255.f, Color[2] / 255.f, 0.f };

    bgfx::setUniform(BorderParamsUniform, Params);
    bgfx::setUniform(BorderColorUniform, BorderColor);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void AddBorders::Destroy()
{
    if (bgfx::isValid(BorderColorUniform))
        bgfx::destroy(BorderColorUniform);
    
    if (bgfx::isValid(BorderParamsUniform))
        bgfx::destroy(BorderParamsUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    BorderColorUniform = BGFX_INVALID_HANDLE;
    BorderParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json AddBorders::Serialize() const
{
    return {
        { kColor, JsonUtil::ColorToJson(Color) },
        { kSize,  Size },
    };
}

void AddBorders::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColor(j, kColor, Color);
    JsonUtil::ReadInt  (j, kSize,  Size);
}
