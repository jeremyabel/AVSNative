#include "ui/ConfigUiRegistry.h"

#include "effects/Interferences.h"

#include <imgui.h>

static void SliderF(const char* label, const char* id, float* v, float lo, float hi)
{
    ImGui::Text(label);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat(id, v, lo, hi);
}

static void DrawInterferencesUI(Effect* base)
{
    auto* fx = static_cast<Interferences*>(base);

    ImGui::Text("Num Points");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##nPoints", &fx->NPoints, 0, 8);

    SliderF("Alpha",               "##alpha",        &fx->Alpha,        1.0f,  255.0f);
    SliderF("Distance",            "##distance",     &fx->Distance,     1.0f,   64.0f);
    SliderF("Rotation Speed",      "##rotationinc",  &fx->RotationInc, -32.0f,  32.0f);
    SliderF("Alpha (On Beat)",     "##alpha2",       &fx->BeatAlpha,       1.0f,  255.0f);
    SliderF("Distance (On Beat)",  "##distance2",    &fx->BeatDistance,    1.0f,   64.0f);
    SliderF("Rot Speed (On Beat)", "##rotationinc2", &fx->BeatRotationInc, -32.0f, 32.0f);
    SliderF("Initial Rotation",    "##rotation",     &fx->Rotation,     0.0f,  255.0f);
    SliderF("Beat Speed",          "##speed",        &fx->Speed,        0.01f,  1.28f);

    ImGui::Checkbox("On Beat", &fx->EnableOnBeatChange);
    ImGui::Checkbox("RGB Separation", &fx->EnableRGB);
    ImGui::Checkbox("Reverse Rotation", &fx->ReverseRotation);

    static const char* kBlends[] = { "Replace", "Additive", "Average" };
    ImGui::Text("Output Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##outBlend", &fx->OutBlend, kBlends, IM_ARRAYSIZE(kBlends));
}

void RegisterInterferencesUI(ConfigUiRegistry& reg)
{
    reg.Register("Interferences", &DrawInterferencesUI);
}
