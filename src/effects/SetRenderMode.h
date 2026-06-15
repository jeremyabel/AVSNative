#pragma once

#include "engine/Effect.h"

// Sets Context.LineMode (width / alpha / blend) for downstream line-drawing effects
// (Simple, etc.). The original win32 r_linemode.cpp packed these into g_line_blend_mode.
class SetRenderMode : public Effect
{
public:


    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Set Render Mode"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Control-only: writes Context.LineBlendMode, produces no image. Uses no views.
    uint8_t ExpectedViewCount() const override { return 0; }

public:

    int BlendMode = 0; // 0=Replace 1=Add 2=Max 3=50/50 4=Sub1 5=Sub2 6=Mul 7=Adjustable 8=XOR 9=Min
    int LineWidth = 1; // 1-255
    int Alpha = 0; // 0-255 (Adjustable blend only)

};
