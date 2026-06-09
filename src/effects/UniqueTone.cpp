#include "UniqueTone.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_uniquetone.sc.bin.h"

#include <algorithm>

void UniqueTone::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_uniquetone_spv, sizeof(fs_uniquetone_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform    = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform  = bgfx::createUniform("u_utColor",  bgfx::UniformType::Vec4);
    ParamsUniform = bgfx::createUniform("u_utParams", bgfx::UniformType::Vec4);
}

void UniqueTone::Render(const RenderContext& Context)
{
    const float color[4]  = { Cfg.Color[0] / 255.0f, Cfg.Color[1] / 255.0f, Cfg.Color[2] / 255.0f, 0.0f };
    const float params[4] = { Cfg.Invert ? 1.0f : 0.0f, (float)Cfg.OutBlend, 0.0f, 0.0f };

    bgfx::setUniform(ColorUniform,  color);
    bgfx::setUniform(ParamsUniform, params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void UniqueTone::Destroy()
{
    if (bgfx::isValid(ParamsUniform)) bgfx::destroy(ParamsUniform);
    if (bgfx::isValid(ColorUniform))  bgfx::destroy(ColorUniform);
    if (bgfx::isValid(TexUniform))    bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))       bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    ColorUniform  = BGFX_INVALID_HANDLE;
    TexUniform    = BGFX_INVALID_HANDLE;
    Program       = BGFX_INVALID_HANDLE;
}
