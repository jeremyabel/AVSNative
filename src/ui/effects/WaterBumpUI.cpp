#include "ui/ConfigUiRegistry.h"

#include "effects/WaterBump.h"

#include <imgui.h>

static void DrawWaterBumpUI(Effect* base)
{
    auto* fx = static_cast<WaterBump*>(base);

    ImGui::TextUnformatted("Fluidity");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##fluidity", &fx->Fluidity, 2, 10);

    ImGui::TextUnformatted("Depth");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##depth", &fx->Depth, 100, 2000);

    ImGui::Checkbox("Random Drop", &fx->Random);

    static const char* kXPos[] = { "Left", "Center", "Right" };
    ImGui::TextUnformatted("Drop X");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##dropPositionX", &fx->DropPositionX, kXPos, IM_ARRAYSIZE(kXPos));

    static const char* kYPos[] = { "Top", "Center", "Bottom" };
    ImGui::TextUnformatted("Drop Y");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##dropPositionY", &fx->DropPositionY, kYPos, IM_ARRAYSIZE(kYPos));

    ImGui::TextUnformatted("Drop Radius");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##dropRadius", &fx->DropRadius, 10, 100);
}

void RegisterWaterBumpUI(ConfigUiRegistry& reg)
{
    reg.Register("Water Bump", &DrawWaterBumpUI);
}
