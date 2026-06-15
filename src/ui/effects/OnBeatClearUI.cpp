#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/OnBeatClear.h"

#include <imgui.h>

static void DrawOnBeatClearUI(Effect* base)
{
    auto* fx = static_cast<OnBeatClear*>(base);

    ImGui::Text("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);

    ImGui::Checkbox("Blend", &fx->Blend);

    ImGui::Text("Every N Beats");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##nf", &fx->ClearEveryN, 0, 100);
}

void RegisterOnBeatClearUI(ConfigUiRegistry& reg)
{
    reg.Register("OnBeat Clear", &DrawOnBeatClearUI);
}
