#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/MovingParticle.h"

#include <imgui.h>

static void DrawMovingParticleUI(Effect* base)
{
    auto* fx = static_cast<MovingParticle*>(base);

    ImGui::TextUnformatted("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);

    ImGui::TextUnformatted("Distance");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##distance", &fx->Distance, 1, 32);

    ImGui::TextUnformatted("Size");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##size", &fx->Size, 1, 128);

    ImGui::Checkbox("On Beat Size Change", &fx->OnBeatSizeChange);

    ImGui::TextUnformatted("On Beat Size");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##onBeatSize", &fx->OnBeatSize, 1, 128);

    static const char* kBlendModes[] = { "Replace", "Additive", "50/50", "Default" };
    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &fx->BlendMode, kBlendModes, IM_ARRAYSIZE(kBlendModes));
}

void RegisterMovingParticleUI(ConfigUiRegistry& reg)
{
    reg.Register("MovingParticle", &DrawMovingParticleUI);
}
