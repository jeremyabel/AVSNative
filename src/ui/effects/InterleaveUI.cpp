#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Interleave.h"

#include <imgui.h>

static void DrawInterleaveUI(Effect* base)
{
    auto* fx = static_cast<Interleave*>(base);

    ImGui::Text("X Size");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##x", &fx->X, 0.0f, 64.0f))
        fx->ResetAnim();

    ImGui::Text("Y Size");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##y", &fx->Y, 0.0f, 64.0f))
        fx->ResetAnim();

    ImGui::Checkbox("On Beat", &fx->EnableOnBeat);

    ImGui::Text("X Size (On Beat)");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##x2", &fx->X2, 0.0f, 64.0f);

    ImGui::Text("Y Size (On Beat)");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##y2", &fx->Y2, 0.0f, 64.0f);

    ImGui::Text("Beat Duration");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##beatdur", &fx->BeatDuration, 1, 64);

    ImGui::Text("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);

    static const char* kBlends[] = { "Replace", "Additive", "Average" };
    ImGui::Text("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##outBlend", &fx->OutBlend, kBlends, IM_ARRAYSIZE(kBlends));
}

void RegisterInterleaveUI(ConfigUiRegistry& reg)
{
    reg.Register("Interleave", &DrawInterleaveUI);
}
