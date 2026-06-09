#include "MultiFilter.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_multifilter.sc.bin.h"

#include <algorithm>

void MultiFilter::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_multifilter_spv, sizeof(fs_multifilter_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform    = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_mfParams", bgfx::UniformType::Vec4);
}

void MultiFilter::Render(const RenderContext& Context)
{
    if (Cfg.ToggleOnBeat && Context.IsBeat())
        ToggleState = !ToggleState;

    // Pass-through: return without swapping so the input propagates unchanged
    if (!ToggleState)
        return;

    const float params[4] = {
        float(Cfg.EffectMode),
        1.0f / float(Context.Width),
        1.0f / float(Context.Height),
        0.0f
    };

    bgfx::setUniform(ParamsUniform, params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void MultiFilter::Destroy()
{
    if (bgfx::isValid(ParamsUniform)) bgfx::destroy(ParamsUniform);
    if (bgfx::isValid(TexUniform))    bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))       bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform    = BGFX_INVALID_HANDLE;
    Program       = BGFX_INVALID_HANDLE;
}
