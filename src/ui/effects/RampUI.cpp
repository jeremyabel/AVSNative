#include "ui/ConfigUiRegistry.h"

#include "effects/Ramp.h"

#include <imgui.h>

static void DrawRampUI(Effect* effect)
{
    auto* fx = static_cast<Ramp*>(effect);

    const char* typeOpts[] = { "Linear", "Radial", "Diamond", "Square" };
    ImGui::Text("Type");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##type", typeOpts[std::clamp(fx->Type, 0, 3)]))
    {
        for (int i = 0; i < 4; ++i)
            if (ImGui::Selectable(typeOpts[i], fx->Type == i)) 
                fx->Type = i;
        ImGui::EndCombo();
    }

    ImGui::Text("Scale");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##scale", &fx->Scale, 0.1f, 8.0f, "%.2f");

    ImGui::Text("Rotation");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##rot", &fx->Rotation, 0.0f, 360.0f, "%.0f deg");

    ImGui::Text("Offset");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##offset", &fx->Offset, -1.0f, 1.0f, "%.2f");

    ImGui::BeginDisabled(fx->Type == 0);
    ImGui::Checkbox("Use window aspect ratio", &fx->UseWindowAspect);
    ImGui::EndDisabled();

    const char* blendOpts[] = { "Replace", "Additive", "50/50" };
    ImGui::Text("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##blend", blendOpts[std::clamp(fx->BlendMode, 0, 2)]))
    {
        for (int i = 0; i < 3; ++i)
            if (ImGui::Selectable(blendOpts[i], fx->BlendMode == i)) 
                fx->BlendMode = i;
        ImGui::EndCombo();
    }
}

void RegisterRampUI(ConfigUiRegistry& reg)
{
    reg.Register("Ramp", DrawRampUI);
}
