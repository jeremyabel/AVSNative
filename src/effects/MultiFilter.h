#pragma once

#include "engine/Effect.h"

class MultiFilter : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Multi Filter"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    int EffectMode = 0;
    bool ToggleOnBeat = false;

private:

    bool ToggleState = true;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
