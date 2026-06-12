#pragma once

#include "engine/Effect.h"

#include <array>

class ColorClip : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::array<uint8_t, 3> Color = { 32, 32, 32 };

    static constexpr const char* kColor = "color";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Color Clip"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
};
