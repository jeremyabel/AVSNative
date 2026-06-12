#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Brightness.h"

#include <imgui.h>

static void DrawBrightnessUI(Effect* base)
{
    auto* fx = static_cast<Brightness*>(base);

    static const char* kBlends[] = { "Replace", "Additive", "50/50" };
    ImGui::TextUnformatted("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blend", &fx->Blend, kBlends, IM_ARRAYSIZE(kBlends));

    // When Separate is off, the three channel sliders move together (matches the
    // original AVS "RGB linked" behavior).
    ImGui::TextUnformatted("Red");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##red", &fx->Red, -4096, 4096) && !fx->Separate)
        fx->Green = fx->Blue = fx->Red;

    ImGui::TextUnformatted("Green");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##green", &fx->Green, -4096, 4096) && !fx->Separate)
        fx->Red = fx->Blue = fx->Green;

    ImGui::TextUnformatted("Blue");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##blue", &fx->Blue, -4096, 4096) && !fx->Separate)
        fx->Red = fx->Green = fx->Blue;

    ImGui::Checkbox("Separate RGB", &fx->Separate);
    ImGui::Checkbox("Exclude Color", &fx->Exclude);

    ImGui::TextUnformatted("Exclude Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##excludeColor", fx->ExcludeColor);

    ImGui::TextUnformatted("Exclude Distance");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##distance", &fx->Distance, 0, 255);
}

void RegisterBrightnessUI(ConfigUiRegistry& reg)
{
    reg.Register("Brightness", &DrawBrightnessUI);
}
