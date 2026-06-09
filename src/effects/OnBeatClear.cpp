#include "OnBeatClear.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_onbeatclear.sc.bin.h"

#include <algorithm>

void OnBeatClear::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_onbeatclear_spv, sizeof(fs_onbeatclear_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform   = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_obcColor", bgfx::UniformType::Vec4);
}

void OnBeatClear::Render(const RenderContext& Context)
{
    if (Context.IsBeat)
    {
        // nf=0 disables the effect entirely (matches `if (nf && ++cf >= nf)`)
        if (Cfg.Nf && ++Cf >= Cfg.Nf)
        {
            Cf = Df = 0;

            const float color[4] = {
                Cfg.Color[0] / 255.0f,
                Cfg.Color[1] / 255.0f,
                Cfg.Color[2] / 255.0f,
                Cfg.Blend ? 1.0f : 0.0f
            };

            bgfx::setUniform(ColorUniform, color);
            bgfx::setTexture(0, TexUniform, Context.InputTexture);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::setVertexBuffer(0, Context.QuadVB);
            bgfx::submit(Context.ViewId, Program);

            Context.FboManager->Swap();
        }
    }
    else
    {
        if (++Df >= Cfg.Nf) Df = 0;
    }
}

void OnBeatClear::Destroy()
{
    if (bgfx::isValid(ColorUniform)) bgfx::destroy(ColorUniform);
    if (bgfx::isValid(TexUniform))   bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))      bgfx::destroy(Program);

    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform   = BGFX_INVALID_HANDLE;
    Program      = BGFX_INVALID_HANDLE;
}
