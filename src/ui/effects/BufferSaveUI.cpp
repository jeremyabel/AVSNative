#include "ui/ConfigUiRegistry.h"

#include "effects/BufferSave.h"

#include <imgui.h>

static void DrawBufferSaveUI(Effect* base)
{
    auto* fx = static_cast<BufferSave*>(base);

    static const char* kModes[] = { "Save", "Restore",
                                    "Alternate Save/Restore", "Alternate Restore/Save" };
    ImGui::TextUnformatted("Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##mode", &fx->Mode, kModes, IM_ARRAYSIZE(kModes));

    static const char* kSlots[] = { "0", "1", "2", "3", "4", "5", "6", "7" };
    ImGui::TextUnformatted("Buffer Slot");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##slot", &fx->Slot, kSlots, IM_ARRAYSIZE(kSlots));

    static const char* kBlends[] = { "Replace", "Additive", "Maximum", "50/50", "Multiply",
                                     "Subtractive 1", "Adjustable", "Minimum",
                                     "Every Other Pixel", "Every Other Line",
                                     "Subtractive 2", "XOR" };
    ImGui::TextUnformatted("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &fx->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::TextUnformatted("Blend Amount");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##blendAmt", &fx->BlendAmt, 0.0f, 1.0f);
}

void RegisterBufferSaveUI(ConfigUiRegistry& reg)
{
    reg.Register("Buffer Save", &DrawBufferSaveUI);
}
