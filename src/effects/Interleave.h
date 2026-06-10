#pragma once

#include "engine/Reflect.h"

struct InterleaveConfig
{
    float                  X        = 1.0f;
    float                  Y        = 1.0f;
    float                  X2       = 1.0f;
    float                  Y2       = 1.0f;
    int                    BeatDur  = 4;
    std::array<uint8_t, 3> Color    = { 0, 0, 0 };
    bool                   OnBeat   = false;
    int                    OutBlend = 0;
};

class Interleave : public ReflectedEffect<InterleaveConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Range(&InterleaveConfig::X, "x", "X Size", 0.0f, 64.0f, 1.0f),
            Range(&InterleaveConfig::Y, "y", "Y Size", 0.0f, 64.0f, 1.0f),
            Range(&InterleaveConfig::X2, "x2", "X Size (On Beat)", 0.0f, 64.0f, 1.0f),
            Range(&InterleaveConfig::Y2, "y2", "Y Size (On Beat)", 0.0f, 64.0f, 1.0f),
            RangeI(&InterleaveConfig::BeatDur, "beatdur", "Beat Duration", 1, 64),
            Color(&InterleaveConfig::Color, "color", "Color"),
            Bool(&InterleaveConfig::OnBeat, "onbeat", "On Beat"),
            SelectI(&InterleaveConfig::OutBlend, "outBlend", "Blend",
                    { "Replace", "Additive", "Average" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Interleave"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override
    {
        for (const std::string& k : changed)
        {
            if (k == "x") CurX = Cfg.X;
            if (k == "y") CurY = Cfg.Y;
        }
    }

private:
    // Runtime state (animated positions)
    float CurX = 1.0f;
    float CurY = 1.0f;

    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle GridUniform  = BGFX_INVALID_HANDLE;
};
