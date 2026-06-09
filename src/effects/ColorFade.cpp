#include "ColorFade.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_colorfade.sc.bin.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

void ColorFade::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    const bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle fs = bgfx::createShader(bgfx::copy(fs_colorfade_spv,  sizeof(fs_colorfade_spv)));
    Program       = bgfx::createProgram(vs, fs, true);
    TexUniform    = bgfx::createUniform("s_texColor",  bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_cfParams",  bgfx::UniformType::Vec4);

    m_fp[0] = (float)Cfg.Faders[0];
    m_fp[1] = (float)Cfg.Faders[1];
    m_fp[2] = (float)Cfg.Faders[2];
}

void ColorFade::UpdateFaderPos(bool isBeat)
{
    if (!Cfg.Gradual)
    {
        m_fp[0] = (float)Cfg.Faders[0];
        m_fp[1] = (float)Cfg.Faders[1];
        m_fp[2] = (float)Cfg.Faders[2];
        return;
    }

    // Gradual: interpolate ±1 per frame toward targets.
    // Note the deliberate cross-mapping from the original: fp[1] tracks Faders[2]
    // and fp[2] tracks Faders[1].
    auto approach = [](float& pos, float target) {
        if      (pos < target) pos = std::min(pos + 1.0f, target);
        else if (pos > target) pos = std::max(pos - 1.0f, target);
    };
    approach(m_fp[0], (float)Cfg.Faders[0]);
    approach(m_fp[1], (float)Cfg.Faders[2]); // cross-mapped
    approach(m_fp[2], (float)Cfg.Faders[1]); // cross-mapped

    if (isBeat)
    {
        if (Cfg.RandomBeat)
        {
            // rand() % 33 gives [0,32]; subtract 6 → [-6, 26]
            for (int i = 0; i < 3; i++)
                m_fp[i] = (float)(rand() % 33 - 6);
        }
        else
        {
            m_fp[0] = (float)Cfg.BeatFaders[0];
            m_fp[1] = (float)Cfg.BeatFaders[1];
            m_fp[2] = (float)Cfg.BeatFaders[2];
        }
    }
}

void ColorFade::Render(const RenderContext& Context)
{
    UpdateFaderPos(Context.IsBeat);

    // Convert from [0,64] fader space (32=neutral) to integer deltas [-32,+32]
    const float params[4] = {
        m_fp[0] - 32.0f,
        m_fp[1] - 32.0f,
        m_fp[2] - 32.0f,
        0.0f
    };
    bgfx::setUniform(ParamsUniform, params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void ColorFade::Destroy()
{
    if (bgfx::isValid(ParamsUniform)) bgfx::destroy(ParamsUniform);
    if (bgfx::isValid(TexUniform))    bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))       bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform    = BGFX_INVALID_HANDLE;
    Program       = BGFX_INVALID_HANDLE;
}
