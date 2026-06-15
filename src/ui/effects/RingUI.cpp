#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Ring.h"

#include <imgui.h>

static void DrawRingUI(Effect* base)
{
    auto* fx = static_cast<Ring*>(base);

    ImGui::Text("Colors");
    ConfigUi::ColorsEdit("##colors", fx->Colors);

    ImGui::Text("Size");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##size", &fx->Size, 1, 64);

    static const char* kSources[] = { "Waveform", "Spectrum" };
    ImGui::Text("Audio Source");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##audioSource", &fx->AudioSource, kSources, IM_ARRAYSIZE(kSources));

    static const char* kChannels[] = { "Left", "Right", "Center" };
    ImGui::Text("Audio Channel");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##audioChannel", &fx->AudioChannel, kChannels, IM_ARRAYSIZE(kChannels));

    static const char* kPositions[] = { "Left", "Right", "Center" };
    ImGui::Text("Position");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##position", &fx->Position, kPositions, IM_ARRAYSIZE(kPositions));
}

void RegisterRingUI(ConfigUiRegistry& reg)
{
    reg.Register("Ring", &DrawRingUI);
}
