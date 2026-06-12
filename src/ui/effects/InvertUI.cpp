#include "ui/ConfigUiRegistry.h"

#include <imgui.h>

static void DrawInvertUI(Effect* /*base*/)
{
    ImGui::TextDisabled("No parameters.");
}

void RegisterInvertUI(ConfigUiRegistry& reg)
{
    reg.Register("Invert", &DrawInvertUI);
}
