#pragma once

#include "engine/Effect.h"

#include <array>
#include <vector>

class DotGrid : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::vector<std::array<uint8_t, 3>> Colors = { { { 255, 255, 255 } } };
    int Spacing   = 8;     // 2–64
    int SpeedX    = 128;   // fixed-point 8.8: 128 = 0.5 px/frame; -512–544
    int SpeedY    = 128;
    int BlendMode = 3;

    static constexpr const char* kColors      = "colors";
    static constexpr const char* kColorLegacy = "color";  // legacy alias = Colors[0]
    static constexpr const char* kSpacing     = "spacing";
    static constexpr const char* kSpeedX      = "speedX";
    static constexpr const char* kSpeedY      = "speedY";
    static constexpr const char* kBlendMode   = "blendMode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Dot Grid"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Restarts the color cycle. Called after Deserialize and by the UI when the
    // color list changes.
    void ResetColorCycle() { ColorPos = 0; }

private:
    // Runtime state
    int32_t Xp       = 0;   // fixed-point 8.8 scroll accumulator
    int32_t Yp       = 0;
    int32_t ColorPos = 0;   // 0 .. Colors.size()*64 - 1

    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle GridUniform  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle SizeUniform  = BGFX_INVALID_HANDLE;
};
