#include "ColorClip.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_colorclip.sc.bin.h"

void ColorClip::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    const bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle fs = bgfx::createShader(bgfx::copy(fs_colorclip_spv,  sizeof(fs_colorclip_spv)));
    Program      = bgfx::createProgram(vs, fs, true);
    TexUniform   = bgfx::createUniform("s_texColor",  bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_clipColor", bgfx::UniformType::Vec4);
}

void ColorClip::Render(const RenderContext& Context)
{
    const float clip[4] = { Cfg.Color[0] / 255.0f, Cfg.Color[1] / 255.0f, Cfg.Color[2] / 255.0f, 0.0f };
    bgfx::setUniform(ColorUniform, clip);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void ColorClip::Destroy()
{
    if (bgfx::isValid(ColorUniform)) bgfx::destroy(ColorUniform);
    if (bgfx::isValid(TexUniform))   bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))      bgfx::destroy(Program);

    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform   = BGFX_INVALID_HANDLE;
    Program      = BGFX_INVALID_HANDLE;
}
