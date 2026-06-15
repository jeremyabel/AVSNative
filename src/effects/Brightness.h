#pragma once

#include "engine/Effect.h"

#include <array>

// Brightness — independent per-channel R/G/B scaling, with a blend mode and an optional
// color-exclusion region. Faithful to the original vis_avs e_brightness.cpp (NOT the
// AVSWeb port, which was a brightness+contrast filter).
//
// Per channel, the multiplier is  1 + (cfg<0 ? 1 : 16) * (cfg / 4096):
//   cfg = -4096 → ×0   (full darken)
//   cfg = 0     → ×1   (no change)
//   cfg = +4096 → ×17  (strong brighten)
class Brightness : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    [[nodiscard]] std::string Name() const override { return "Brightness"; }
    [[nodiscard]] nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    int Blend = 2; // 0 = Replace, 1 = Additive, 2 = 50/50 (default)
    int Red = 0;
    int Green = 0;
    int Blue = 0;
    bool EnableExcludeColor = false;
    std::array<uint8_t, 3> ExcludeColor = { 0, 0, 0 };
    int Distance = 16;

private:

    bgfx::ProgramHandle Program  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle InputUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle MultColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ExcludeColorUniform = BGFX_INVALID_HANDLE;
};
