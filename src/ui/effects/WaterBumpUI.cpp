#include "ui/ConfigUiRegistry.h"

#include "effects/WaterBump.h"

#include <imgui.h>

static void DrawWaterBumpUI(Effect* base)
{
    auto* fx = static_cast<WaterBump*>(base);

    ImGui::Text("Fluidity");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##fluidity", &fx->Fluidity, 2, 10);

    ImGui::Text("Depth");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##depth", &fx->Depth, 100, 2000);

    ImGui::Checkbox("Random Drop", &fx->Random);

    static const char* kXPos[] = { "Left", "Center", "Right" };
    ImGui::Text("Drop X");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##dropPositionX", &fx->DropPositionX, kXPos, IM_ARRAYSIZE(kXPos));

    static const char* kYPos[] = { "Top", "Center", "Bottom" };
    ImGui::Text("Drop Y");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##dropPositionY", &fx->DropPositionY, kYPos, IM_ARRAYSIZE(kYPos));

    ImGui::Text("Drop Radius");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##dropRadius", &fx->DropRadius, 10, 100);
}

void RegisterWaterBumpUI(ConfigUiRegistry& reg)
{
    reg.Register("Water Bump", &DrawWaterBumpUI);
}
