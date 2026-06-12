#include "ui/ConfigUiRegistry.h"

#include "effects/Grain.h"

#include <imgui.h>

static void DrawGrainUI(Effect* base)
{
    auto* fx = static_cast<Grain*>(base);

    ImGui::TextUnformatted("Amount");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##amount", &fx->Amount, 0, 100);

    static const char* kBlendModes[] = { "Replace", "Additive", "50/50" };
    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &fx->BlendMode, kBlendModes, IM_ARRAYSIZE(kBlendModes));

    ImGui::Checkbox("Static", &fx->IsStatic);
}

void RegisterGrainUI(ConfigUiRegistry& reg)
{
    reg.Register("Grain", &DrawGrainUI);
}
