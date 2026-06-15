#pragma once

#include "engine/Effect.h"

class Scatter : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Scatter"; }
    nlohmann::json Serialize() const override { return nlohmann::json::object(); }
    void Deserialize(const nlohmann::json& /*j*/) override {}

private:

    uint32_t SeedFrame = 0;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
