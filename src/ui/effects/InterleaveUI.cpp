#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Interleave.h"

#include <imgui.h>

static void DrawInterleaveUI(Effect* base)
{
    auto* fx = static_cast<Interleave*>(base);

    ImGui::TextUnformatted("X Size");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##x", &fx->X, 0.0f, 64.0f))
        fx->ResetAnim();

    ImGui::TextUnformatted("Y Size");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##y", &fx->Y, 0.0f, 64.0f))
        fx->ResetAnim();

    ImGui::TextUnformatted("X Size (On Beat)");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##x2", &fx->X2, 0.0f, 64.0f);

    ImGui::TextUnformatted("Y Size (On Beat)");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##y2", &fx->Y2, 0.0f, 64.0f);

    ImGui::TextUnformatted("Beat Duration");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##beatdur", &fx->BeatDur, 1, 64);

    ImGui::TextUnformatted("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);

    ImGui::Checkbox("On Beat", &fx->OnBeat);

    static const char* kBlends[] = { "Replace", "Additive", "Average" };
    ImGui::TextUnformatted("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##outBlend", &fx->OutBlend, kBlends, IM_ARRAYSIZE(kBlends));
}

void RegisterInterleaveUI(ConfigUiRegistry& reg)
{
    reg.Register("Interleave", &DrawInterleaveUI);
}
