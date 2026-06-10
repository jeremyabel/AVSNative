#pragma once

#include "engine/Reflect.h"

struct ColorFadeConfig
{
    int  Faders[3]     = { 32, 32, 32 }; // 0-64, 32 = no change
    int  BeatFaders[3] = { 32, 32, 32 };
    bool Gradual       = false;
    bool RandomBeat    = false;
};

class ColorFade : public ReflectedEffect<ColorFadeConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeIArr(&ColorFadeConfig::Faders, 0, "fader0", "Fader 1", 0, 64),
            RangeIArr(&ColorFadeConfig::Faders, 1, "fader1", "Fader 2", 0, 64),
            RangeIArr(&ColorFadeConfig::Faders, 2, "fader2", "Fader 3", 0, 64),
            RangeIArr(&ColorFadeConfig::BeatFaders, 0, "beat_fader0", "Beat Fader 1", 0, 64),
            RangeIArr(&ColorFadeConfig::BeatFaders, 1, "beat_fader1", "Beat Fader 2", 0, 64),
            RangeIArr(&ColorFadeConfig::BeatFaders, 2, "beat_fader2", "Beat Fader 3", 0, 64),
            Bool(&ColorFadeConfig::Gradual, "gradual", "Gradual"),
            Bool(&ColorFadeConfig::RandomBeat, "random_beat", "Random on Beat"),
        };
        return f;
    }
    std::string EffectName() const override { return "Colorfade"; }

    void OnConfigChanged(const std::vector<std::string>& /*changed*/) override
    {
        // Reset interpolated positions to match new config.
        m_fp[0] = (float)Cfg.Faders[0];
        m_fp[1] = (float)Cfg.Faders[1];
        m_fp[2] = (float)Cfg.Faders[2];
    }

private:
    void UpdateFaderPos(bool isBeat);

    // Runtime state (not serialized)
    float m_fp[3] = { 32.0f, 32.0f, 32.0f }; // interpolated fader positions

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
