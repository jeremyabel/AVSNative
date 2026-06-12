#include "ui/ConfigUiRegistry.h"

#include "effects/Multiplier.h"

#include <imgui.h>

static void DrawMultiplierUI(Effect* base)
{
    auto* fx = static_cast<Multiplier*>(base);

    static const char* kModes[] = { "Inv (non-black to white)", "x8", "x4", "x2",
                                    "x1/2", "x1/4", "x1/8", "XS (white only)" };
    ImGui::TextUnformatted("Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##mode", &fx->Mode, kModes, IM_ARRAYSIZE(kModes));
}

void RegisterMultiplierUI(ConfigUiRegistry& reg)
{
    reg.Register("Multiplier", &DrawMultiplierUI);
}
