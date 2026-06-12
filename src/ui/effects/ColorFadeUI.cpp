#include "ui/ConfigUiRegistry.h"

#include "effects/ColorFade.h"

#include <imgui.h>

static void DrawColorFadeUI(Effect* base)
{
    auto* fx = static_cast<ColorFade*>(base);

    static const char* kFaderLabels[3] = { "Fader 1", "Fader 2", "Fader 3" };
    for (int i = 0; i < 3; ++i)
    {
        ImGui::PushID(i);
        ImGui::TextUnformatted(kFaderLabels[i]);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##fader", &fx->Faders[i], -32, 32))
            fx->ResetFaderPos();
        ImGui::PopID();
    }

    static const char* kBeatFaderLabels[3] = { "Beat Fader 1", "Beat Fader 2", "Beat Fader 3" };
    for (int i = 0; i < 3; ++i)
    {
        ImGui::PushID(100 + i);
        ImGui::TextUnformatted(kBeatFaderLabels[i]);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##beatFader", &fx->BeatFaders[i], -32, 32);
        ImGui::PopID();
    }

    ImGui::Checkbox("Gradual", &fx->Gradual);
    ImGui::Checkbox("Random on Beat", &fx->RandomBeat);
}

void RegisterColorFadeUI(ConfigUiRegistry& reg)
{
    reg.Register("Colorfade", &DrawColorFadeUI);
}
