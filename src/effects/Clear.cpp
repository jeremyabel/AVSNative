#include "Clear.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_clear.sc.bin.h"

#include <algorithm>

void Clear::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_clear_spv, sizeof(fs_clear_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    ColorUniform = bgfx::createUniform("u_clearColor", bgfx::UniformType::Vec4);
}

void Clear::Render(const RenderContext& Context)
{
    const float color[4] = { Cfg.Color[0] / 255.0f, Cfg.Color[1] / 255.0f, Cfg.Color[2] / 255.0f, 1.0f };

    bgfx::setViewFrameBuffer(Context.ViewId, Context.FboManager->GetNext().Fbo);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)Context.Width, (uint16_t)Context.Height);

    bgfx::setUniform(ColorUniform, color);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Clear::Destroy()
{
    if (bgfx::isValid(ColorUniform)) bgfx::destroy(ColorUniform);
    if (bgfx::isValid(Program))      bgfx::destroy(Program);

    ColorUniform = BGFX_INVALID_HANDLE;
    Program      = BGFX_INVALID_HANDLE;
}
