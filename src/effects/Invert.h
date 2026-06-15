#pragma once

#include "engine/Effect.h"

class Invert : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Invert"; }
    nlohmann::json Serialize() const override { return nlohmann::json::object(); }
    void Deserialize(const nlohmann::json& /*j*/) override {}

private:

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
};
