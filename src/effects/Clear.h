#pragma once

#include "engine/Effect.h"

#include <array>

class Clear : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::array<uint8_t, 3> Color = { 0, 0, 0 };

    static constexpr const char* kColor = "color";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Clear"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
};
