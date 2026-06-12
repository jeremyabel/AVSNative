#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/OscilloscopeStar.h"

#include <imgui.h>

static void DrawOscilloscopeStarUI(Effect* base)
{
    auto* fx = static_cast<OscilloscopeStar*>(base);

    ImGui::TextUnformatted("Colors");
    ConfigUi::ColorsEdit("##colors", fx->Colors);

    static const char* kChannels[] = { "Left", "Right", "Center" };
    ImGui::TextUnformatted("Audio Channel");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##audioChannel", &fx->AudioChannel, kChannels, IM_ARRAYSIZE(kChannels));

    static const char* kPositions[] = { "Left", "Right", "Center" };
    ImGui::TextUnformatted("Position");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##position", &fx->Position, kPositions, IM_ARRAYSIZE(kPositions));

    ImGui::TextUnformatted("Size");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##size", &fx->Size, 0, 32);

    ImGui::TextUnformatted("Rotation Speed");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##rotation", &fx->Rotation, -16, 16);
}

void RegisterOscilloscopeStarUI(ConfigUiRegistry& reg)
{
    reg.Register("Oscilloscope Star", &DrawOscilloscopeStarUI);
}
