#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/BassSpin.h"

#include <imgui.h>

static void DrawBassSpinUI(Effect* base)
{
    auto* fx = static_cast<BassSpin*>(base);

    ImGui::Checkbox("Enabled Left", &fx->EnabledLeft);
    ImGui::Checkbox("Enabled Right", &fx->EnabledRight);

    ImGui::Text("Color Left");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##colorLeft", fx->ColorLeft);

    ImGui::Text("Color Right");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##colorRight", fx->ColorRight);

    ImGui::Text("Mode");
    ImGui::RadioButton("Outline", &fx->Mode, 0); ImGui::SameLine();
    ImGui::RadioButton("Filled", &fx->Mode, 1);
}

void RegisterBassSpinUI(ConfigUiRegistry& reg)
{
    reg.Register("Bass Spin", &DrawBassSpinUI);
}
