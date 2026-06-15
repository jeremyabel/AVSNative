#pragma once

#include "engine/Effect.h"

#include <array>
#include <cstdint>

// Timescope — a scrolling spectrogram. Each frame draws one vertical column from the
// spectrum (vertical axis = frequency bin, brightness = magnitude), advancing the
// column position horizontally so the image scrolls.
class Timescope : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Timescope"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    int Channel = 2; // 0 = Left, 1 = Right, 2 = Center (avg) — spectrum
    std::array<uint8_t, 3> Color = { 255, 255, 255 };
    int Blend = 3; // 0 = Replace, 1 = Additive, 2 = 50/50, 3 = Default (= Replace)
    int Bands = 576; // spectrum bins spread across the column height (16..576)

private:

    int LastPosition = 0;
    int LastWidth = 0; // last width, to reset position on resize

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle AudioUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params1Uniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params2Uniform = BGFX_INVALID_HANDLE;
};
