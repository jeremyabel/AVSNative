#pragma once

#include "engine/Effect.h"

class Interferences : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Interferences"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    int NPoints = 2; 
    float Distance = 10.0f;
    float Alpha = 128.0f; 
    float Rotation = 0.0f; 
    float RotationInc = 0.0f;
    float BeatDistance = 32.0f;
    float BeatAlpha = 192.0f;
    float BeatRotationInc = 25.0f;
    bool EnableRGB = true;
    int OutBlend = 0;
    bool EnableOnBeatChange = true;
    float Speed = 0.2f;
    bool ReverseRotation = true;

private:

    float Phase = 3.14159265358979323846f;  // beat oscillation phase; starts at π (resting)

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets0Uniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets1Uniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets2Uniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets3Uniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
