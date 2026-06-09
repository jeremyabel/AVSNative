#include "FadeOut.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_fadeout.sc.bin.h"

void FadeOut::Init(bgfx::RendererType::Enum Renderer)
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_fadeout_spv, sizeof(fs_fadeout_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    FadeParamsUniform = bgfx::createUniform("u_fadeParams", bgfx::UniformType::Vec4);
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
}

void FadeOut::Render(const RenderContext& Context)
{
    // Speed maps directly to blend amount: 0 = no fade, 1 = instant wipe to color.
    // speed=0.08 → 8% per frame toward target, equivalent to AVS_Remake fade=0.92.
    const float Params[4] = { Cfg.Color[0] / 255.0f, Cfg.Color[1] / 255.0f, Cfg.Color[2] / 255.0f, Cfg.Speed };

    bgfx::setUniform(FadeParamsUniform, Params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void FadeOut::Destroy()
{
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(FadeParamsUniform))
        bgfx::destroy(FadeParamsUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    TexUniform = BGFX_INVALID_HANDLE;
    FadeParamsUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}
