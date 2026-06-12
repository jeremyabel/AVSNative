#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/DotFountain.h"

#include <imgui.h>

static void DrawDotFountainUI(Effect* base)
{
    auto* fx = static_cast<DotFountain*>(base);

    std::array<uint8_t, 3>* colors[5] = { &fx->Color0, &fx->Color1, &fx->Color2,
                                          &fx->Color3, &fx->Color4 };
    static const char* kLabels[5] = { "Color 1", "Color 2", "Color 3", "Color 4", "Color 5" };
    for (int i = 0; i < 5; ++i)
    {
        ImGui::PushID(i);
        ImGui::TextUnformatted(kLabels[i]);
        ImGui::SetNextItemWidth(-1.0f);
        if (ConfigUi::ColorEdit("##color", *colors[i]))
            fx->BuildColorMap();
        ImGui::PopID();
    }

    ImGui::TextUnformatted("Rotation Speed");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##rotationSpeed", &fx->RotationSpeed, -50, 50);

    ImGui::TextUnformatted("Tilt Angle");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##angle", &fx->Angle, -90, 91);
}

void RegisterDotFountainUI(ConfigUiRegistry& reg)
{
    reg.Register("Dot Fountain", &DrawDotFountainUI);
}
