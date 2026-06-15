#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/OscilloscopeStar.h"

#include <imgui.h>

static void DrawOscilloscopeStarUI(Effect* base)
{
    auto* fx = static_cast<OscilloscopeStar*>(base);

    ImGui::Text("Colors");
    ConfigUi::ColorsEdit("##colors", fx->Colors);

    ImGui::Text("Audio Channel");
    ImGui::RadioButton("Left##AudioChannel", &fx->AudioChannel, 0); ImGui::SameLine();
    ImGui::RadioButton("Right##AudioChannel", &fx->AudioChannel, 1); ImGui::SameLine();
    ImGui::RadioButton("Center##AudioChannel", &fx->AudioChannel, 2);

    ImGui::Text("Position");
    ImGui::RadioButton("Left##Position", &fx->Position, 0); ImGui::SameLine();
    ImGui::RadioButton("Right##Position", &fx->Position, 1); ImGui::SameLine();
    ImGui::RadioButton("Center##Position", &fx->Position, 2);

    ImGui::Text("Size");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##size", &fx->Size, 0, 32);

    ImGui::Text("Rotation Speed");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##rotation", &fx->Rotation, -16, 16);
}

void RegisterOscilloscopeStarUI(ConfigUiRegistry& reg)
{
    reg.Register("Oscilloscope Star", &DrawOscilloscopeStarUI);
}
