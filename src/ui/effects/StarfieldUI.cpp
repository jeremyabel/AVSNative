#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Starfield.h"

#include <imgui.h>

static void DrawStarfieldUI(Effect* base)
{
    auto* fx = static_cast<Starfield*>(base);

    ImGui::Text("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);

    static const char* kBlends[] = { "Replace", "Additive", "50/50" };
    ImGui::Text("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &fx->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::Text("Warp Speed");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##speed", &fx->Speed, 1.0f, 500.0f))
        fx->ResetSpeed();

    ImGui::Text("Stars Count");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##starCount", &fx->StarCount, 100, 4095))
        fx->ReinitStars();

    ImGui::Checkbox("On Beat", &fx->OnBeat);
    if (fx->OnBeat)
    {
        ImGui::Text("On-Beat Speed");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##onBeatSpeed", &fx->OnBeatSpeed, 1.0f, 500.0f);

        ImGui::Text("On-Beat Duration");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##onBeatDuration", &fx->OnBeatDuration, 1, 100);
    }
}

void RegisterStarfieldUI(ConfigUiRegistry& reg)
{
    reg.Register("Starfield", &DrawStarfieldUI);
}
