#include "ui/ConfigUiRegistry.h"

#include "effects/ChannelShift.h"

#include <imgui.h>

static void DrawChannelShiftUI(Effect* base)
{
    auto* fx = static_cast<ChannelShift*>(base);

    static const char* kModes[] = { "RGB (none)", "RBG", "GRB", "GBR", "BRG", "BGR" };
    ImGui::TextUnformatted("Channel Order");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##mode", &fx->Mode, kModes, IM_ARRAYSIZE(kModes));

    ImGui::Checkbox("On Beat Random", &fx->OnBeatRandom);
}

void RegisterChannelShiftUI(ConfigUiRegistry& reg)
{
    reg.Register("Channel Shift", &DrawChannelShiftUI);
}
