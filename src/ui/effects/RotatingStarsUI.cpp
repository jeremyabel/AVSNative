#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/RotatingStars.h"

#include <imgui.h>

static void DrawRotatingStarsUI(Effect* base)
{
    auto* fx = static_cast<RotatingStars*>(base);

    ImGui::TextUnformatted("Colors");
    ConfigUi::ColorsEdit("##colors", fx->Colors);
}

void RegisterRotatingStarsUI(ConfigUiRegistry& reg)
{
    reg.Register("Rotating Stars", &DrawRotatingStarsUI);
}
