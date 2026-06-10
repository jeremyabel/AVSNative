#include "Scatter.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_scatter.sc.bin.h"

static constexpr float k_phi = 1.6180339887f;

void Scatter::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_scatter_spv, sizeof(fs_scatter_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform    = bgfx::createUniform("s_texColor",     bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_scatterParams", bgfx::UniformType::Vec4);
}

void Scatter::Render(const RenderContext& Context)
{
    SeedFrame = (SeedFrame + 1) & 0xFFFF;
    float seed = (float)SeedFrame * k_phi;

    const float params[4] = {
        (float)Context.Width,
        (float)Context.Height,
        seed,
        0.0f,
    };

    bgfx::setUniform(ParamsUniform, params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Scatter::Destroy()
{
    if (bgfx::isValid(ParamsUniform)) bgfx::destroy(ParamsUniform);
    if (bgfx::isValid(TexUniform))    bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))       bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform    = BGFX_INVALID_HANDLE;
    Program       = BGFX_INVALID_HANDLE;
}
