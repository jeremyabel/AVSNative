#include "ui/ConfigUiRegistry.h"

#include "effects/FastBrightness.h"

#include <imgui.h>

static void DrawFastBrightnessUI(Effect* base)
{
    auto* fx = static_cast<FastBrightness*>(base);

    static const char* kModes[] = { "\xC3\x97""2 Brighter", "\xC3\x97\xC2\xBD Darker", "No Change" };
    ImGui::TextUnformatted("Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##dir", &fx->Dir, kModes, IM_ARRAYSIZE(kModes));
}

void RegisterFastBrightnessUI(ConfigUiRegistry& reg)
{
    reg.Register("Fast Brightness", &DrawFastBrightnessUI);
}
