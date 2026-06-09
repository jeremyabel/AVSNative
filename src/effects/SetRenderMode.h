#pragma once

#include "engine/Reflect.h"

struct SetRenderModeConfig
{
    int BlendMode = 0; // 0=Replace 1=Add 2=Max 3=50/50 4=Sub1 5=Sub2 6=Mul 7=Adjustable 8=XOR 9=Min
    int LineWidth = 1; // 1-255
    int Alpha     = 0; // 0-255 (Adjustable blend only)
};

// Sets Context.LineBlendMode for downstream line-drawing effects (Simple, etc.).
// Matches g_line_blend_mode in r_linemode.cpp:
//   bits 16-23: lineWidth  (1-255)
//   bits  8-15: alpha      (0-255, Adjustable blend mode)
//   bits  0-7:  blendMode  (0-9)
class SetRenderMode : public ReflectedEffect<SetRenderModeConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    // Control-only: writes Context.LineBlendMode, produces no image. Uses no views.
    uint8_t ExpectedViewCount() const override { return 0; }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&SetRenderModeConfig::BlendMode, "blend_mode", "Blend Mode",
                    { "Replace", "Add", "Max", "50/50", "Sub 1", "Sub 2", "Multiply",
                      "Adjustable", "XOR", "Minimum" }),
            RangeI(&SetRenderModeConfig::LineWidth, "line_width", "Line Width", 1, 255),
            RangeI(&SetRenderModeConfig::Alpha, "alpha", "Alpha", 0, 255),
        };
        return f;
    }
    std::string EffectName() const override { return "Set Render Mode"; }
};
