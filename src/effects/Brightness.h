#pragma once

#include "engine/Reflect.h"

#include <array>
#include <cstdint>
#include <vector>

// Brightness — independent per-channel R/G/B scaling, with a blend mode and an optional
// color-exclusion region. Faithful to the original vis_avs e_brightness.cpp (NOT the
// AVSWeb port, which was a brightness+contrast filter).
//
// Per channel, the multiplier is  1 + (cfg<0 ? 1 : 16) * (cfg / 4096):
//   cfg = -4096 → ×0   (full darken)
//   cfg = 0     → ×1   (no change)
//   cfg = +4096 → ×17  (strong brighten)

struct BrightnessConfig
{
    int Blend = 2;        // 0 = Replace, 1 = Additive, 2 = 50/50 (default)
    int Red   = 0;        // -4096..4096
    int Green = 0;
    int Blue  = 0;
    bool Separate = false;  // UI hint: edit channels independently (render uses all 3 regardless)
    bool Exclude  = false;  // skip pixels near ExcludeColor
    std::array<uint8_t, 3> ExcludeColor = { 0, 0, 0 };
    int Distance = 16;    // 0..255 exclusion radius (per channel)
};

class Brightness : public ReflectedEffect<BrightnessConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&BrightnessConfig::Blend, "blend", "Blend", { "Replace", "Additive", "50/50" }),
            RangeI(&BrightnessConfig::Red,   "red",   "Red",   -4096, 4096),
            RangeI(&BrightnessConfig::Green, "green", "Green", -4096, 4096),
            RangeI(&BrightnessConfig::Blue,  "blue",  "Blue",  -4096, 4096),
            Bool(&BrightnessConfig::Separate, "separate", "Separate RGB"),
            Bool(&BrightnessConfig::Exclude,  "exclude",  "Exclude Color"),
            Color(&BrightnessConfig::ExcludeColor, "excludeColor", "Exclude Color"),
            RangeI(&BrightnessConfig::Distance, "distance", "Exclude Distance", 0, 255),
        };
        return f;
    }
    std::string EffectName() const override { return "Brightness"; }

private:
    bgfx::ProgramHandle m_program  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uInput   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uMult    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uParams  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uExclude = BGFX_INVALID_HANDLE;
};
