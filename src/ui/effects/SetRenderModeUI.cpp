#include "ui/ConfigUiRegistry.h"

#include "effects/SetRenderMode.h"

#include <imgui.h>

static void DrawSetRenderModeUI(Effect* base)
{
    auto* fx = static_cast<SetRenderMode*>(base);

    ImGui::Text("Line Width");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##lineWidth", &fx->LineWidth, 1, 255);

    static const char* kBlends[] = { "Replace", "Add", "Max", "50/50", "Sub 1", "Sub 2", "Multiply", "Adjustable", "XOR", "Minimum" };
    ImGui::Text("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &fx->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));

    if (fx->BlendMode == 7) // Adjustable blend
    {
        ImGui::Text("Alpha");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##alpha", &fx->Alpha, 0, 255);
    }
}

void RegisterSetRenderModeUI(ConfigUiRegistry& reg)
{
    reg.Register("Set Render Mode", &DrawSetRenderModeUI);
}
