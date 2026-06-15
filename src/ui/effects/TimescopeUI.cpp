#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Timescope.h"

#include <imgui.h>

static void DrawTimescopeUI(Effect* base)
{
    auto* fx = static_cast<Timescope*>(base);

    static const char* kChannels[] = { "Left", "Right", "Center" };
    ImGui::Text("Source");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##channel", &fx->Channel, kChannels, IM_ARRAYSIZE(kChannels));

    ImGui::Text("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);

    static const char* kBlends[] = { "Replace", "Additive", "50/50", "Default" };
    ImGui::Text("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blend", &fx->Blend, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::Text("Bands");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##bands", &fx->Bands, 16, 576);
}

void RegisterTimescopeUI(ConfigUiRegistry& reg)
{
    reg.Register("Timescope", &DrawTimescopeUI);
}
