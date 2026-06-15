#include "ui/ConfigUiRegistry.h"

#include "effects/ColorReduction.h"

#include <imgui.h>

static void DrawColorReductionUI(Effect* base)
{
    auto* fx = static_cast<ColorReduction*>(base);

    ImGui::Text("Levels (bits)");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##levels", &fx->Levels, 1, 8);
}

void RegisterColorReductionUI(ConfigUiRegistry& reg)
{
    reg.Register("Color Reduction", &DrawColorReductionUI);
}
