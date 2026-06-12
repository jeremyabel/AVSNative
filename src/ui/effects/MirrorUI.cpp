#include "ui/ConfigUiRegistry.h"

#include "effects/Mirror.h"

#include <imgui.h>

static void DrawMirrorUI(Effect* base)
{
    auto* fx = static_cast<Mirror*>(base);

    ImGui::Checkbox("Flip Horizontal", &fx->FlipX);
    ImGui::Checkbox("Flip Vertical", &fx->FlipY);
    ImGui::Checkbox("Toggle on Beat", &fx->OnBeat);
}

void RegisterMirrorUI(ConfigUiRegistry& reg)
{
    reg.Register("Mirror", &DrawMirrorUI);
}
