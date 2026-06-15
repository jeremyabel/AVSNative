#include "ui/ConfigUiRegistry.h"

#include "effects/FastBrightness.h"

#include <imgui.h>

static void DrawFastBrightnessUI(Effect* base)
{
    auto* fx = static_cast<FastBrightness*>(base);

    ImGui::Text("Mode");
    ImGui::RadioButton("\xC3\x97""2 Brighter", &fx->Dir, 0);
    ImGui::RadioButton("\xC3\x97\xC2\xBD Darker", &fx->Dir, 1);
    ImGui::RadioButton("No Change", &fx->Dir, 2);
}

void RegisterFastBrightnessUI(ConfigUiRegistry& reg)
{
    reg.Register("Fast Brightness", &DrawFastBrightnessUI);
}
