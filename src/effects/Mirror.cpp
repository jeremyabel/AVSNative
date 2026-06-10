#include "Mirror.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_mirror.sc.bin.h"

void Mirror::Init()
{
    const bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle fs = bgfx::createShader(bgfx::copy(fs_mirror_spv,     sizeof(fs_mirror_spv)));
    Program       = bgfx::createProgram(vs, fs, true);
    TexUniform    = bgfx::createUniform("s_texColor",    bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_mirrorParams", bgfx::UniformType::Vec4);
}

void Mirror::Render(const RenderContext& Context)
{
    if (Cfg.OnBeat && Context.IsBeat())
        m_beatActive = !m_beatActive;

    bool fx = Cfg.FlipX, fy = Cfg.FlipY;
    if (Cfg.OnBeat && m_beatActive) { fx = !fx; fy = !fy; }

    const int mode = (fx ? 1 : 0) | (fy ? 2 : 0);
    const float params[4] = { (float)mode, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(ParamsUniform, params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Mirror::Destroy()
{
    if (bgfx::isValid(ParamsUniform)) bgfx::destroy(ParamsUniform);
    if (bgfx::isValid(TexUniform))    bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))       bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform    = BGFX_INVALID_HANDLE;
    Program       = BGFX_INVALID_HANDLE;
}
