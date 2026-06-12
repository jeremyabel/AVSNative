#include "ui/ConfigUiRegistry.h"

#include "effects/MultiFilter.h"

#include <imgui.h>

static void DrawMultiFilterUI(Effect* base)
{
    auto* fx = static_cast<MultiFilter*>(base);

    static const char* kModes[] = { "Chrome", "Double Chrome", "Triple Chrome",
                                    "Infroot + Border Convolution" };
    ImGui::TextUnformatted("Effect");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##effect", &fx->EffectMode, kModes, IM_ARRAYSIZE(kModes));

    ImGui::Checkbox("Toggle On Beat", &fx->ToggleOnBeat);
}

void RegisterMultiFilterUI(ConfigUiRegistry& reg)
{
    reg.Register("Multi Filter", &DrawMultiFilterUI);
}
