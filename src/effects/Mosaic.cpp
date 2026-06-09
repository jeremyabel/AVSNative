#include "Mosaic.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_mosaic.sc.bin.h"

void Mosaic::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    const bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle fs = bgfx::createShader(bgfx::copy(fs_mosaic_spv,     sizeof(fs_mosaic_spv)));
    Program       = bgfx::createProgram(vs, fs, true);
    TexUniform    = bgfx::createUniform("s_texColor",    bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_mosaicParams", bgfx::UniformType::Vec4);
}

void Mosaic::Render(const RenderContext& Context)
{
    const float params[4] = {
        (float)Cfg.BlockSize,
        (float)Context.Width,
        (float)Context.Height,
        0.0f
    };
    bgfx::setUniform(ParamsUniform, params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Mosaic::Destroy()
{
    if (bgfx::isValid(ParamsUniform)) bgfx::destroy(ParamsUniform);
    if (bgfx::isValid(TexUniform))    bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))       bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform    = BGFX_INVALID_HANDLE;
    Program       = BGFX_INVALID_HANDLE;
}
