#pragma once

#include "engine/Effect.h"

#include <array>

class FadeOut : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    float                  Speed = 1.0f;  // 0–1
    std::array<uint8_t, 3> Color = { 0, 0, 0 };

    static constexpr const char* kSpeed = "speed";
    static constexpr const char* kColor = "color";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "FadeOut"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program           = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle FadeParamsUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform        = BGFX_INVALID_HANDLE;
};
