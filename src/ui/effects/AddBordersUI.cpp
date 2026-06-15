#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/AddBorders.h"

#include <imgui.h>

static void DrawAddBordersUI(Effect* base)
{
    auto* fx = static_cast<AddBorders*>(base);

    ImGui::Text("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);

    ImGui::Text("Size");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##size", &fx->Size, 1, 50);
}

void RegisterAddBordersUI(ConfigUiRegistry& reg)
{
    reg.Register("Add Borders", &DrawAddBordersUI);
}
