#include "Mosaic.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_mosaic.sc.bin.h"

#include <algorithm>
#include <cstdlib>

void Mosaic::Init()
{
    const bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle fs = bgfx::createShader(bgfx::copy(fs_mosaic_spv,     sizeof(fs_mosaic_spv)));
    Program       = bgfx::createProgram(vs, fs, true);
    TexUniform    = bgfx::createUniform("s_texColor",    bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_mosaicParams", bgfx::UniformType::Vec4);

    m_curSize = Cfg.Size;
}

void Mosaic::Render(const RenderContext& Context)
{
    // ── On-beat size selection + cooldown decay (ported from e_mosaic.cpp). ──────
    if (Cfg.OnBeatSizeChange && Context.IsBeat())
    {
        m_curSize  = Cfg.OnBeatSize;
        m_cooldown = Cfg.OnBeatDuration;
    }
    else if (m_cooldown == 0)
    {
        m_curSize = Cfg.Size;
    }

    if (m_cooldown > 0)
    {
        m_cooldown--;
        if (m_cooldown > 0)
        {
            const int dur = std::max(1, Cfg.OnBeatDuration);
            const int a   = std::abs(Cfg.Size - Cfg.OnBeatSize) / dur;
            m_curSize += a * (Cfg.OnBeatSize > Cfg.Size ? -1 : 1);
        }
    }

    const int cs = std::clamp(m_curSize, 1, 100);

    const float params[4] = {
        (float)cs,
        (float)Context.Width,
        (float)Context.Height,
        (float)Cfg.Blend,
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
