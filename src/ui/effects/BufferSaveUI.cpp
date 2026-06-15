#include "ui/ConfigUiRegistry.h"

#include "effects/BufferSave.h"

#include <imgui.h>

static void DrawBufferSaveUI(Effect* base)
{
    auto* fx = static_cast<BufferSave*>(base);

    ImGui::Text("Mode");
    ImGui::RadioButton("Save", &fx->Mode, 0);
    ImGui::RadioButton("Restore", &fx->Mode, 1);
    ImGui::RadioButton("Alternate Save/Restore", &fx->Mode, 2);
    ImGui::RadioButton("Alternate Restore/Save", &fx->Mode, 3);

    static const char* kSlots[] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    ImGui::Text("Buffer Slot");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##slot", &fx->Slot, kSlots, IM_ARRAYSIZE(kSlots));

    static const char* kBlends[] = { "Replace", "Additive", "Maximum", "50/50", "Multiply",
                                     "Subtractive 1", "Adjustable", "Minimum",
                                     "Every Other Pixel", "Every Other Line",
                                     "Subtractive 2", "XOR" };
    ImGui::Text("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &fx->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));

    if (fx->BlendMode == 6)
    {
        ImGui::Text("Blend Amount");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##blendAmt", &fx->BlendAmt, 0.0f, 1.0f);
    }
}

void RegisterBufferSaveUI(ConfigUiRegistry& reg)
{
    reg.Register("Buffer Save", &DrawBufferSaveUI);
}
