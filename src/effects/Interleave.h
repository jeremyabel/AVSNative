#pragma once

#include "engine/Effect.h"

#include <array>

class Interleave : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Interleave"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Snaps the animated grid position to X/Y. Called after Deserialize and by
    // the UI when the X/Y sliders change.
    void ResetAnim() { CurX = X; CurY = Y; }

public:

    float X = 1.0f;
    float Y = 1.0f;
    float X2 = 1.0f;
    float Y2 = 1.0f;
    int BeatDuration = 4;
    std::array<uint8_t, 3> Color = { 0, 0, 0 };
    bool EnableOnBeat = false;
    int OutBlend = 0;

private:

    float CurX = 1.0f;
    float CurY = 1.0f;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle GridUniform = BGFX_INVALID_HANDLE;
};
