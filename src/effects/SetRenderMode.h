#pragma once

#include "engine/Effect.h"

// Sets Context.LineBlendMode for downstream line-drawing effects (Simple, etc.).
// Matches g_line_blend_mode in r_linemode.cpp:
//   bits 16-23: lineWidth  (1-255)
//   bits  8-15: alpha      (0-255, Adjustable blend mode)
//   bits  0-7:  blendMode  (0-9)
class SetRenderMode : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int BlendMode = 0; // 0=Replace 1=Add 2=Max 3=50/50 4=Sub1 5=Sub2 6=Mul 7=Adjustable 8=XOR 9=Min
    int LineWidth = 1; // 1-255
    int Alpha     = 0; // 0-255 (Adjustable blend only)

    static constexpr const char* kBlendMode = "blend_mode";
    static constexpr const char* kLineWidth = "line_width";
    static constexpr const char* kAlpha     = "alpha";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Set Render Mode"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Control-only: writes Context.LineBlendMode, produces no image. Uses no views.
    uint8_t ExpectedViewCount() const override { return 0; }
};
