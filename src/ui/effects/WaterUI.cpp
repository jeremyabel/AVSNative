#include "ui/ConfigUiRegistry.h"

#include <imgui.h>

static void DrawWaterUI(Effect* /*base*/)
{
    ImGui::TextDisabled("No parameters.");
}

void RegisterWaterUI(ConfigUiRegistry& reg)
{
    reg.Register("Water", &DrawWaterUI);
}
