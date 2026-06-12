#include "ui/ConfigUiRegistry.h"

#include <imgui.h>

static void DrawNormalizeUI(Effect* /*base*/)
{
    ImGui::TextDisabled("No parameters.");
}

void RegisterNormalizeUI(ConfigUiRegistry& reg)
{
    reg.Register("Normalize", &DrawNormalizeUI);
}
