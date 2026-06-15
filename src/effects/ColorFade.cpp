#include "ColorFade.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_colorfade.sc.bin.h"

#include <algorithm>
#include <cstdlib>

static constexpr const char* NAME_Fader0 = "fader0";
static constexpr const char* NAME_Fader1 = "fader1";
static constexpr const char* NAME_Fader2 = "fader2";
static constexpr const char* NAME_BeatFader0 = "beat_fader0";
static constexpr const char* NAME_BeatFader1 = "beat_fader1";
static constexpr const char* NAME_BeatFader2 = "beat_fader2";
static constexpr const char* NAME_EnableOnBeatChange = "gradual";
static constexpr const char* NAME_EnableRandomBeat = "random_beat";

void ColorFade::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_colorfade_spv, sizeof(fs_colorfade_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);
    
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_cfParams", bgfx::UniformType::Vec4);

    InterpFaders[0] = (float)Faders[0];
    InterpFaders[1] = (float)Faders[1];
    InterpFaders[2] = (float)Faders[2];
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

void ColorFade::UpdateFaderPos(bool IsBeat)
{
    if (!EnableOnBeatChange)
    {
        InterpFaders[0] = (float)Faders[0];
        InterpFaders[1] = (float)Faders[1];
        InterpFaders[2] = (float)Faders[2];
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
    approach(InterpFaders[0], (float)Faders[0]);
    approach(InterpFaders[1], (float)Faders[1]);
    approach(InterpFaders[2], (float)Faders[2]);

    if (IsBeat)
    {
        if (EnableRandomBeat)
        {
            // Matches colorfade.js: outer faders land in [-6,25], the middle one
            // in [-32,31] snapped to the extremes when it falls near neutral.
            InterpFaders[0] = (float)(rand() % 32 - 6);
            InterpFaders[2] = (float)(rand() % 32 - 6);

            int v = rand() % 64 - 32;
            if      (v < 0  && v > -16) v = -32;
            else if (v >= 0 && v <  16) v =  32;
            InterpFaders[1] = (float)v;
        }
        else
        {
            InterpFaders[0] = (float)BeatFaders[0];
            InterpFaders[1] = (float)BeatFaders[1];
            InterpFaders[2] = (float)BeatFaders[2];
        }
    }
}

void ColorFade::Render(const RenderContext& Context)
{
    UpdateFaderPos(Context.IsBeat());

    const float uParams[4] = { InterpFaders[0], InterpFaders[1], InterpFaders[2], 0.0f };
    
    // Point-sample: Colorfade is a 1:1 per-pixel transform, and bilinear filtering
    // introduces sub-LSB per-channel noise that flips the hard channel-dominance
    // classification (a neutral gray would otherwise be pushed into a colour branch).
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture, BGFX_SAMPLER_POINT);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json ColorFade::Serialize() const
{
    return 
    {
        { NAME_Fader0, Faders[0] },
        { NAME_Fader1, Faders[1] },
        { NAME_Fader2, Faders[2] },
        { NAME_BeatFader0, BeatFaders[0] },
        { NAME_BeatFader1, BeatFaders[1] },
        { NAME_BeatFader2, BeatFaders[2] },
        { NAME_EnableOnBeatChange, EnableOnBeatChange },
        { NAME_EnableRandomBeat, EnableRandomBeat },
    };
}

void ColorFade::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Fader0, Faders[0]);
    JsonUtil::ReadInt(j, NAME_Fader1, Faders[1]);
    JsonUtil::ReadInt(j, NAME_Fader2, Faders[2]);
    JsonUtil::ReadInt(j, NAME_BeatFader0, BeatFaders[0]);
    JsonUtil::ReadInt(j, NAME_BeatFader1, BeatFaders[1]);
    JsonUtil::ReadInt(j, NAME_BeatFader2, BeatFaders[2]);
    JsonUtil::ReadBool(j, NAME_EnableOnBeatChange, EnableOnBeatChange);
    JsonUtil::ReadBool(j, NAME_EnableRandomBeat, EnableRandomBeat);

    ResetFaderPos();
}
