#pragma once

#include "engine/Reflect.h"

struct InterferencesConfig
{
    int   NPoints      = 2;
    float Distance     = 10.0f;
    float Alpha        = 128.0f;
    float Rotation     = 0.0f;   // initial rotation; advanced at runtime in Render
    float RotationInc  = 0.0f;
    float Distance2    = 32.0f;
    float Alpha2       = 192.0f;
    float RotationInc2 = 25.0f;
    bool  RGB          = true;
    int   OutBlend     = 0;
    bool  OnBeat       = true;
    float Speed        = 0.2f;
};

class Interferences : public ReflectedEffect<InterferencesConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI(&InterferencesConfig::NPoints, "nPoints", "Num Points", 0, 8),
            Range(&InterferencesConfig::Alpha, "alpha", "Alpha", 1.0f, 255.0f, 1.0f),
            Range(&InterferencesConfig::Distance, "distance", "Distance", 1.0f, 64.0f, 1.0f),
            Range(&InterferencesConfig::RotationInc, "rotationinc", "Rotation Speed", -32.0f, 32.0f, 1.0f),
            Range(&InterferencesConfig::Alpha2, "alpha2", "Alpha (On Beat)", 1.0f, 255.0f, 1.0f),
            Range(&InterferencesConfig::Distance2, "distance2", "Distance (On Beat)", 1.0f, 64.0f, 1.0f),
            Range(&InterferencesConfig::RotationInc2, "rotationinc2", "Rot Speed (On Beat)", -32.0f, 32.0f, 1.0f),
            Range(&InterferencesConfig::Rotation, "rotation", "Initial Rotation", 0.0f, 255.0f, 1.0f),
            Range(&InterferencesConfig::Speed, "speed", "Beat Speed", 0.01f, 1.28f, 0.01f),
            Bool(&InterferencesConfig::OnBeat, "onbeat", "On Beat"),
            Bool(&InterferencesConfig::RGB, "rgb", "RGB Separation"),
            SelectI(&InterferencesConfig::OutBlend, "outBlend", "Output Blend",
                    { "Replace", "Additive", "Average" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Interferences"; }

private:
    // Runtime state
    float Status = 3.14159265358979323846f;  // beat oscillation phase; starts at π (resting)

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets0      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets1      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets2      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets3      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
