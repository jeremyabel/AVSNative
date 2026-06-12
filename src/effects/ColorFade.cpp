#include "ColorFade.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_colorfade.sc.bin.h"

#include <algorithm>
#include <cstdlib>

void ColorFade::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_colorfade_spv, sizeof(fs_colorfade_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);
    
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_cfParams", bgfx::UniformType::Vec4);

    m_fp[0] = (float)Faders[0];
    m_fp[1] = (float)Faders[1];
    m_fp[2] = (float)Faders[2];
}

void ColorFade::UpdateFaderPos(bool isBeat)
{
    if (!Gradual)
    {
        m_fp[0] = (float)Faders[0];
        m_fp[1] = (float)Faders[1];
        m_fp[2] = (float)Faders[2];
        return;
    }

    // Gradual: interpolate ±1 per frame toward targets. The win32 original's
    // default version has each cur_X track its own fader_X (only the special
    // V2_81D version swaps cur_max <-> cur_3rd_gray). Tracking the wrong target
    // here corrupts cur_3rd_gray, the fader used for neutral/dark pixels.
    auto approach = [](float& pos, float target) {
        if      (pos < target) pos = std::min(pos + 1.0f, target);
        else if (pos > target) pos = std::max(pos - 1.0f, target);
    };
    approach(m_fp[0], (float)Faders[0]);
    approach(m_fp[1], (float)Faders[1]);
    approach(m_fp[2], (float)Faders[2]);

    if (isBeat)
    {
        if (RandomBeat)
        {
            // Matches colorfade.js: outer faders land in [-6,25], the middle one
            // in [-32,31] snapped to the extremes when it falls near neutral.
            m_fp[0] = (float)(rand() % 32 - 6);
            m_fp[2] = (float)(rand() % 32 - 6);

            int v = rand() % 64 - 32;
            if      (v < 0  && v > -16) v = -32;
            else if (v >= 0 && v <  16) v =  32;
            m_fp[1] = (float)v;
        }
        else
        {
            m_fp[0] = (float)BeatFaders[0];
            m_fp[1] = (float)BeatFaders[1];
            m_fp[2] = (float)BeatFaders[2];
        }
    }
}

void ColorFade::Render(const RenderContext& Context)
{
    UpdateFaderPos(Context.IsBeat());

    // Faders are signed deltas in [-32,+32] (0 = no change), passed straight to
    // the shader exactly as the JS reference does — no offset.
    const float Params[4] = {
        m_fp[0],
        m_fp[1],
        m_fp[2],
        0.0f
    };
    
    bgfx::setUniform(ParamsUniform, Params);
    // Point-sample: Colorfade is a 1:1 per-pixel transform, and bilinear filtering
    // introduces sub-LSB per-channel noise that flips the hard channel-dominance
    // classification (a neutral gray would otherwise be pushed into a colour branch).
    bgfx::setTexture(0, TexUniform, Context.InputTexture, BGFX_SAMPLER_POINT);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void ColorFade::Destroy()
{
    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json ColorFade::Serialize() const
{
    return {
        { kFader0,     Faders[0]     },
        { kFader1,     Faders[1]     },
        { kFader2,     Faders[2]     },
        { kBeatFader0, BeatFaders[0] },
        { kBeatFader1, BeatFaders[1] },
        { kBeatFader2, BeatFaders[2] },
        { kGradual,    Gradual       },
        { kRandomBeat, RandomBeat    },
    };
}

void ColorFade::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt (j, kFader0,     Faders[0]);
    JsonUtil::ReadInt (j, kFader1,     Faders[1]);
    JsonUtil::ReadInt (j, kFader2,     Faders[2]);
    JsonUtil::ReadInt (j, kBeatFader0, BeatFaders[0]);
    JsonUtil::ReadInt (j, kBeatFader1, BeatFaders[1]);
    JsonUtil::ReadInt (j, kBeatFader2, BeatFaders[2]);
    JsonUtil::ReadBool(j, kGradual,    Gradual);
    JsonUtil::ReadBool(j, kRandomBeat, RandomBeat);

    ResetFaderPos();
}
