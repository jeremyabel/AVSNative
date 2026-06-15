#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/DotPlane.h"

#include <imgui.h>

static void DrawDotPlaneUI(Effect* base)
{
    auto* fx = static_cast<DotPlane*>(base);

    ImGui::Text("Rotation Speed");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##rotationSpeed", &fx->RotationSpeed, -50, 50);

    ImGui::Text("Angle");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##angle", &fx->Angle, -90, 91);

    std::array<uint8_t, 3>* Colors[5] = { &fx->Color0, &fx->Color1, &fx->Color2, &fx->Color3, &fx->Color4 };
    static const char* kLabels[5] = { "Color 1", "Color 2", "Color 3", "Color 4", "Color 5" };
    for (int i = 0; i < 5; ++i)
    {
        ImGui::PushID(i);
        ImGui::Text(kLabels[i]);
        ImGui::SetNextItemWidth(-1.0f);
        if (ConfigUi::ColorEdit("##color", *Colors[i]))
            fx->BuildColorMap();
        ImGui::PopID();
    }
}

void RegisterDotPlaneUI(ConfigUiRegistry& reg)
{
    reg.Register("Dot Plane", &DrawDotPlaneUI);
}
