#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/FadeOut.h"

#include <imgui.h>

static void DrawFadeOutUI(Effect* base)
{
    auto* fx = static_cast<FadeOut*>(base);

    ImGui::Text("Speed");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##speed", &fx->Speed, 0.0f, 1.0f);

    ImGui::Text("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", fx->Color);
}

void RegisterFadeOutUI(ConfigUiRegistry& reg)
{
    reg.Register("FadeOut", &DrawFadeOutUI);
}
