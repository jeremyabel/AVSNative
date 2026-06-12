#pragma once

#include "engine/Effect.h"

class Multiplier : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int Mode = 3;  // default: ×2

    static constexpr const char* kMode = "mode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Multiplier"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
