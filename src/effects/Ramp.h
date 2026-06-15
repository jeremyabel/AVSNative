#pragma once

#include "engine/Effect.h"

#include <bgfx/bgfx.h>

class Ramp : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Ramp"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    int Type = 0; // 0=Linear, 1=Radial, 2=Diamond, 3=Square
    float Scale = 1.0f;
    float Rotation = 0.0f;
    float Offset = 0.0f; 
    int BlendMode = 0;// 0=Replace, 1=Additive, 2=50/50
    bool UseWindowAspect = true;

private:

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params1Uniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params2Uniform = BGFX_INVALID_HANDLE;
};
