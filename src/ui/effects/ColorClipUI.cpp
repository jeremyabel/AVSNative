#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/ColorClip.h"

#include <imgui.h>

static void DrawColorClipUI(Effect* base)
{
    auto* fx = static_cast<ColorClip*>(base);

    ImGui::Text("Clip Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);
}

void RegisterColorClipUI(ConfigUiRegistry& reg)
{
    reg.Register("Color Clip", &DrawColorClipUI);
}
