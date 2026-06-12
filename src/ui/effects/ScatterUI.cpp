#include "ui/ConfigUiRegistry.h"

#include <imgui.h>

static void DrawScatterUI(Effect* /*base*/)
{
    ImGui::TextDisabled("No parameters.");
}

void RegisterScatterUI(ConfigUiRegistry& reg)
{
    reg.Register("Scatter", &DrawScatterUI);
}
