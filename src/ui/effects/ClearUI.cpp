#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Clear.h"

#include <imgui.h>

static void DrawClearUI(Effect* base)
{
    auto* fx = static_cast<Clear*>(base);

    ImGui::Text("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);
}

void RegisterClearUI(ConfigUiRegistry& reg)
{
    reg.Register("Clear", &DrawClearUI);
}
