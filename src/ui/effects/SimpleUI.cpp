#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Simple.h"

#include <imgui.h>

static void DrawSimpleUI(Effect* base)
{
    auto* fx = static_cast<Simple*>(base);

    ImGui::Text("Mode");
    ImGui::RadioButton("Solid Analyzer", &fx->Mode, 0); ImGui::SameLine();
    ImGui::RadioButton("Line Analyzer", &fx->Mode, 1);
    ImGui::RadioButton("Line Scope", &fx->Mode, 2); ImGui::SameLine();
    ImGui::RadioButton("Solid Scope", &fx->Mode, 3);

    ImGui::Text("Channel");
    ImGui::RadioButton("Left##AudioChannel", &fx->Channel, 0); ImGui::SameLine();
    ImGui::RadioButton("Right##AudioChannel", &fx->Channel, 1); ImGui::SameLine();
    ImGui::RadioButton("Center##AudioChannel", &fx->Channel, 2);

    ImGui::Text("Position");
    ImGui::RadioButton("Top##Position", &fx->Position, 0); ImGui::SameLine();
    ImGui::RadioButton("Center##Position", &fx->Position, 1); ImGui::SameLine();
    ImGui::RadioButton("Bottom##Position", &fx->Position, 2);

    ImGui::Text("Color");
    ConfigUi::ColorsEdit("##colors", fx->Colors);

    ImGui::Checkbox("Antialiasing", &fx->AntialiasingEnabled);
}

void RegisterSimpleUI(ConfigUiRegistry& reg)
{
    reg.Register("Simple", &DrawSimpleUI);
}
