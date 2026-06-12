#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/DotGrid.h"

#include <imgui.h>

static void DrawDotGridUI(Effect* base)
{
    auto* fx = static_cast<DotGrid*>(base);

    ImGui::TextUnformatted("Colors");
    if (ConfigUi::ColorsEdit("##colors", fx->Colors))
        fx->ResetColorCycle();

    ImGui::TextUnformatted("Spacing");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##spacing", &fx->Spacing, 2, 64);

    ImGui::TextUnformatted("Speed X");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##speedX", &fx->SpeedX, -512, 544);

    ImGui::TextUnformatted("Speed Y");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##speedY", &fx->SpeedY, -512, 544);

    static const char* kBlends[] = { "Replace", "Additive", "50/50", "Default" };
    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &fx->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));
}

void RegisterDotGridUI(ConfigUiRegistry& reg)
{
    reg.Register("Dot Grid", &DrawDotGridUI);
}
