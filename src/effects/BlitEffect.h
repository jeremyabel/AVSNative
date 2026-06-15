#pragma once

#include "engine/Effect.h"

class BlitEffect : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Blit"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    float Zoom = 1.05f;
    float Rotation = 0.0f;
    float CenterX = 0.5f;
    float CenterY = 0.5f;
    bool Bilinear = false;
    bool Compat = false;

private:

    float Angle = 0.f;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle FlagsUniform = BGFX_INVALID_HANDLE;
};
