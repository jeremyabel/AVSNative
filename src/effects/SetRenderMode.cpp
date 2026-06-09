#include "SetRenderMode.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"

#include <algorithm>

void SetRenderMode::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    const bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle fs = bgfx::createShader(bgfx::copy(fs_blit_spv,       sizeof(fs_blit_spv)));
    Program    = bgfx::createProgram(vs, fs, true);
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
}

void SetRenderMode::Render(const RenderContext& Context)
{
    // Write the packed line blend mode into the shared frame-level state.
    // Downstream effects in the same chain read Context.LineBlendMode each frame.
    if (Context.LineBlendMode)
    {
        *Context.LineBlendMode =
            ((uint32_t)(Cfg.LineWidth & 0xFF) << 16) |
            ((uint32_t)(Cfg.Alpha     & 0xFF) <<  8) |
             (uint32_t)(Cfg.BlendMode & 0xFF);
    }

    // Pass-through blit: SetRenderMode has no visual output of its own, but must
    // consume its pre-allocated view slot and maintain the ping-pong FBO chain.
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void SetRenderMode::Destroy()
{
    if (bgfx::isValid(TexUniform)) bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))    bgfx::destroy(Program);

    TexUniform = BGFX_INVALID_HANDLE;
    Program    = BGFX_INVALID_HANDLE;
}
