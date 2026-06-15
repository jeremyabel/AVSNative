#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/UniqueTone.h"

#include <imgui.h>

static void DrawUniqueToneUI(Effect* base)
{
    auto* fx = static_cast<UniqueTone*>(base);

    ImGui::Text("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);

    ImGui::Checkbox("Invert", &fx->EnableInvert);

    static const char* kBlends[] = { "Replace", "Additive", "Average" };
    ImGui::Text("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##outBlend", &fx->OutBlend, kBlends, IM_ARRAYSIZE(kBlends));
}

void RegisterUniqueToneUI(ConfigUiRegistry& reg)
{
    reg.Register("Unique Tone", &DrawUniqueToneUI);
}
