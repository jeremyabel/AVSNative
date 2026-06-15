#include "ui/ConfigUiRegistry.h"

#include "effects/Blur.h"

#include <imgui.h>

static void DrawBlurUI(Effect* base)
{
    auto* fx = static_cast<Blur*>(base);

    ImGui::Text("Intensity");
    ImGui::RadioButton("Light", &fx->Intensity, 0);
    ImGui::RadioButton("Medium", &fx->Intensity, 1);
    ImGui::RadioButton("Heavy", &fx->Intensity, 2);
}

void RegisterBlurUI(ConfigUiRegistry& reg)
{
    reg.Register("Blur", &DrawBlurUI);
}
