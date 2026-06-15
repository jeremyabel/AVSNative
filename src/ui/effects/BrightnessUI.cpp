#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"
#include "ui/effects/BrightnessUI.h"

#include "effects/Brightness.h"

#include <imgui.h>
#include <unordered_map>

static std::unordered_map<Brightness*, BrightnessUIState> s_state;

static void DrawBrightnessUI(Effect* base)
{
    auto* fx = static_cast<Brightness*>(base);
    BrightnessUIState& ui = s_state[fx];

    static const char* kBlends[] = { "Replace", "Additive", "50/50" };
    ImGui::Text("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blend", &fx->Blend, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::Text("Red");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##red", &fx->Red, -4096, 4096) && !ui.Separate)
        fx->Green = fx->Blue = fx->Red;

    ImGui::Text("Green");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##green", &fx->Green, -4096, 4096) && !ui.Separate)
        fx->Red = fx->Blue = fx->Green;

    ImGui::Text("Blue");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##blue", &fx->Blue, -4096, 4096) && !ui.Separate)
        fx->Red = fx->Green = fx->Blue;

    ImGui::Checkbox("Separate RGB", &ui.Separate);

    ImGui::Checkbox("Exclude Color", &fx->EnableExcludeColor);
    if (fx->EnableExcludeColor)
    {
        ImGui::Text("Exclude Color");
        ImGui::SetNextItemWidth(-1.0f);
        ConfigUi::ColorEdit("##excludeColor", fx->ExcludeColor);

        ImGui::Text("Exclude Distance");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##distance", &fx->Distance, 0, 255);
    }
}

void RegisterBrightnessUI(ConfigUiRegistry& reg)
{
    reg.Register("Brightness", &DrawBrightnessUI);
}
