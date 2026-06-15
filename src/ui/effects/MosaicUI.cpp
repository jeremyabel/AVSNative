#include "ui/ConfigUiRegistry.h"

#include "effects/Mosaic.h"

#include <imgui.h>

static void DrawMosaicUI(Effect* base)
{
    auto* fx = static_cast<Mosaic*>(base);

    ImGui::Text("Size");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##size", &fx->Size, 1, 100);

    ImGui::Checkbox("On-Beat Size Change", &fx->EnableOnBeatSizeChange);
    if (fx->EnableOnBeatSizeChange)
    {
        ImGui::Text("On-Beat Size");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##onBeatSize", &fx->OnBeatSize, 1, 100);
    
        ImGui::Text("On-Beat Duration");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##onBeatDuration", &fx->OnBeatDuration, 1, 100);
    }

    static const char* kBlends[] = { "Replace", "Additive", "50/50" };
    ImGui::Text("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blend", &fx->Blend, kBlends, IM_ARRAYSIZE(kBlends));
}

void RegisterMosaicUI(ConfigUiRegistry& reg)
{
    reg.Register("Mosaic", &DrawMosaicUI);
}
