#include "ui/ConfigUiRegistry.h"

#include "effects/MultiFilter.h"

#include <imgui.h>

static void DrawMultiFilterUI(Effect* base)
{
    auto* fx = static_cast<MultiFilter*>(base);

    ImGui::Text("Effect");
    ImGui::RadioButton("Chrome", &fx->EffectMode, 0);
    ImGui::RadioButton("Double Chrome", &fx->EffectMode, 1);
    ImGui::RadioButton("Triple Chrome", &fx->EffectMode, 2);
    ImGui::RadioButton("Infroot + Border Convolution", &fx->EffectMode, 3);

    ImGui::Checkbox("Toggle On Beat", &fx->ToggleOnBeat);
}

void RegisterMultiFilterUI(ConfigUiRegistry& reg)
{
    reg.Register("Multi Filter", &DrawMultiFilterUI);
}
