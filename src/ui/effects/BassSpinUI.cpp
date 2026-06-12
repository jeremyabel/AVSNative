#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/BassSpin.h"

#include <imgui.h>

static void DrawBassSpinUI(Effect* base)
{
    auto* fx = static_cast<BassSpin*>(base);

    ImGui::Checkbox("Enabled Left", &fx->EnabledLeft);
    ImGui::Checkbox("Enabled Right", &fx->EnabledRight);

    ImGui::TextUnformatted("Color Left");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##colorLeft", fx->ColorLeft);

    ImGui::TextUnformatted("Color Right");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##colorRight", fx->ColorRight);

    static const char* kModes[] = { "Outline", "Filled" };
    ImGui::TextUnformatted("Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##mode", &fx->Mode, kModes, IM_ARRAYSIZE(kModes));
}

void RegisterBassSpinUI(ConfigUiRegistry& reg)
{
    reg.Register("Bass Spin", &DrawBassSpinUI);
}
