#include "ui/ConfigUiRegistry.h"

#include "effects/Multiplier.h"

#include <imgui.h>

static void DrawMultiplierUI(Effect* base)
{
    auto* fx = static_cast<Multiplier*>(base);

    ImGui::Text("Mode");
    ImGui::RadioButton("Inv (non-black to white)", &fx->Mode, 0);
    ImGui::RadioButton("x8", &fx->Mode, 1);
    ImGui::RadioButton("x4", &fx->Mode, 2);
    ImGui::RadioButton("x2", &fx->Mode, 3);
    ImGui::RadioButton("x1/2", &fx->Mode, 4);
    ImGui::RadioButton("x1/4", &fx->Mode, 5);
    ImGui::RadioButton("x1/8", &fx->Mode, 6);
    ImGui::RadioButton("XS (white only)", &fx->Mode, 7);
}

void RegisterMultiplierUI(ConfigUiRegistry& reg)
{
    reg.Register("Multiplier", &DrawMultiplierUI);
}
