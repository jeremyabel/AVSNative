#include "ui/ConfigUiRegistry.h"

#include "effects/Blur.h"

#include <imgui.h>

static void DrawBlurUI(Effect* base)
{
    auto* fx = static_cast<Blur*>(base);

    static const char* kLevels[] = { "Light", "Medium", "Heavy" };
    ImGui::TextUnformatted("Intensity");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##intensity", &fx->Intensity, kLevels, IM_ARRAYSIZE(kLevels));
}

void RegisterBlurUI(ConfigUiRegistry& reg)
{
    reg.Register("Blur", &DrawBlurUI);
}
