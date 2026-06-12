#pragma once

#include "engine/Effect.h"

#include <array>
#include <cstdint>

// Brightness — independent per-channel R/G/B scaling, with a blend mode and an optional
// color-exclusion region. Faithful to the original vis_avs e_brightness.cpp (NOT the
// AVSWeb port, which was a brightness+contrast filter).
//
// Per channel, the multiplier is  1 + (cfg<0 ? 1 : 16) * (cfg / 4096):
//   cfg = -4096 → ×0   (full darken)
//   cfg = 0     → ×1   (no change)
//   cfg = +4096 → ×17  (strong brighten)

class Brightness : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int Blend = 2;        // 0 = Replace, 1 = Additive, 2 = 50/50 (default)
    int Red   = 0;        // -4096..4096
    int Green = 0;
    int Blue  = 0;
    bool Separate = false;  // UI hint: edit channels independently (render uses all 3 regardless)
    bool Exclude  = false;  // skip pixels near ExcludeColor
    std::array<uint8_t, 3> ExcludeColor = { 0, 0, 0 };
    int Distance = 16;    // 0..255 exclusion radius (per channel)

    static constexpr const char* kBlend        = "blend";
    static constexpr const char* kRed          = "red";
    static constexpr const char* kGreen        = "green";
    static constexpr const char* kBlue         = "blue";
    static constexpr const char* kSeparate     = "separate";
    static constexpr const char* kExclude      = "exclude";
    static constexpr const char* kExcludeColor = "excludeColor";
    static constexpr const char* kDistance     = "distance";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Brightness"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle m_program  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uInput   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uMult    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uParams  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uExclude = BGFX_INVALID_HANDLE;
};
