#pragma once

#include "engine/Effect.h"

class ColorReduction : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int Levels = 7; // 1..8 bits per channel; shader receives 2^Levels

    static constexpr const char* kLevels = "levels";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Color Reduction"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
