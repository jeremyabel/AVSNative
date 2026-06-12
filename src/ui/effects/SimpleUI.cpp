#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Simple.h"

#include <imgui.h>

static void DrawSimpleUI(Effect* base)
{
    auto* fx = static_cast<Simple*>(base);

    static const char* kModes[] = { "Solid Analyzer", "Line Analyzer", "Line Scope", "Solid Scope" };
    ImGui::TextUnformatted("Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##mode", &fx->Mode, kModes, IM_ARRAYSIZE(kModes));

    static const char* kChannels[] = { "Left", "Right", "Mono Mix" };
    ImGui::TextUnformatted("Channel");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##channel", &fx->Channel, kChannels, IM_ARRAYSIZE(kChannels));

    static const char* kPositions[] = { "Top", "Center", "Bottom" };
    ImGui::TextUnformatted("Position");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##position", &fx->Position, kPositions, IM_ARRAYSIZE(kPositions));

    ImGui::TextUnformatted("Color");
    ConfigUi::ColorsEdit("##colors", fx->Colors);

    ImGui::Checkbox("Antialiasing", &fx->AntialiasingEnabled);
}

void RegisterSimpleUI(ConfigUiRegistry& reg)
{
    reg.Register("Simple", &DrawSimpleUI);
}
