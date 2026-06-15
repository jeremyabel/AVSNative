#pragma once

#include "engine/Effect.h"

class Mosaic : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Mosaic"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    int Size = 50;
    bool EnableOnBeatSizeChange = false;
    int OnBeatSize = 50;
    int OnBeatDuration = 15;
    int Blend = 0; // 0 = Replace, 1 = Additive, 2 = 50/50

private:

    int CurrentSize = 50;
    int RemainingCooldownTime = 0;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
