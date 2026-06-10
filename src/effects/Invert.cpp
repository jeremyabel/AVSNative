#include "Invert.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_invert.sc.bin.h"

void Invert::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_invert_spv, sizeof(fs_invert_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
}

void Invert::Render(const RenderContext& Context)
{
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Invert::Destroy()
{
    if (bgfx::isValid(TexUniform)) bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))    bgfx::destroy(Program);

    TexUniform = BGFX_INVALID_HANDLE;
    Program    = BGFX_INVALID_HANDLE;
}
