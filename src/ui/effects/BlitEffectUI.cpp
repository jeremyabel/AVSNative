#include "ui/ConfigUiRegistry.h"

#include "effects/BlitEffect.h"

#include <imgui.h>

static void DrawBlitEffectUI(Effect* base)
{
    auto* fx = static_cast<BlitEffect*>(base);

    ImGui::Text("Zoom");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##zoom", &fx->Zoom, 0.8f, 1.5f);

    ImGui::Text("Rotation");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##rotation", &fx->Rotation, -0.1f, 0.1f);

    ImGui::Text("Center X");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##centerX", &fx->CenterX, 0.0f, 1.0f);

    ImGui::Text("Center Y");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##centerY", &fx->CenterY, 0.0f, 1.0f);

    ImGui::Checkbox("Bilinear", &fx->Bilinear);
    if (fx->Bilinear)
    {
        ImGui::SameLine();
        ImGui::Checkbox("Precise", &fx->Compat);
    }
}

void RegisterBlitEffectUI(ConfigUiRegistry& reg)
{
    reg.Register("Blit", &DrawBlitEffectUI);
}
