#include "AddBorders.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_addborders.sc.bin.h"

#include <algorithm>
#include <cmath>

void AddBorders::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_addborders_spv, sizeof(fs_addborders_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    BorderParamsUniform = bgfx::createUniform("u_borderParams", bgfx::UniformType::Vec4);
    BorderColorUniform  = bgfx::createUniform("u_borderColor",  bgfx::UniformType::Vec4);
}

void AddBorders::Render(const RenderContext& Context)
{
    const float W = (float)Context.Width;
    const float H = (float)Context.Height;

    float borderW = std::max(1.0f, std::floor(W * Cfg.Size / 100.0f));
    float borderH = std::max(1.0f, std::floor(H * Cfg.Size / 100.0f));

    const float params[4] = { borderW, borderH, W, H };
    const float color[4] = { Cfg.Color[0] / 255.0f, Cfg.Color[1] / 255.0f, Cfg.Color[2] / 255.0f, 0.0f };

    bgfx::setUniform(BorderParamsUniform, params);
    bgfx::setUniform(BorderColorUniform,  color);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void AddBorders::Destroy()
{
    if (bgfx::isValid(BorderColorUniform))  bgfx::destroy(BorderColorUniform);
    if (bgfx::isValid(BorderParamsUniform)) bgfx::destroy(BorderParamsUniform);
    if (bgfx::isValid(TexUniform))          bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))             bgfx::destroy(Program);

    BorderColorUniform  = BGFX_INVALID_HANDLE;
    BorderParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform          = BGFX_INVALID_HANDLE;
    Program             = BGFX_INVALID_HANDLE;
}
