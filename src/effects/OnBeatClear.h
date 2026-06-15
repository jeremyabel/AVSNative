#pragma once

#include "engine/Effect.h"

#include <array>

class OnBeatClear : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "OnBeat Clear"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    std::array<uint8_t, 3> Color = { 255, 255, 255 };
    bool Blend = false;
    int ClearEveryN = 1;

private:

    int32_t BeatsSinceLastClear = 0;
    int32_t NonBeatFramesSinceLastClear = 0;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
};
