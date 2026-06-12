#pragma once

#include "engine/Effect.h"

#include <array>

class OnBeatClear : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::array<uint8_t, 3> Color = { 255, 255, 255 };
    bool                   Blend = false;
    int                    Nf    = 1;   // clear every N beats (0–100); 0 = disabled

    static constexpr const char* kColor = "color";
    static constexpr const char* kBlend = "blend";
    static constexpr const char* kNf    = "nf";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "OnBeat Clear"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // Runtime counters
    int32_t Cf = 0;   // beats since last clear
    int32_t Df = 0;   // non-beat frames since last clear

    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
};
