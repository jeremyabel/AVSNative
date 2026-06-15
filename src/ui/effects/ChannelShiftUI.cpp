#include "ui/ConfigUiRegistry.h"

#include "effects/ChannelShift.h"

#include <imgui.h>

static void DrawChannelShiftUI(Effect* base)
{
    auto* fx = static_cast<ChannelShift*>(base);

    ImGui::Text("Channel Order");
    ImGui::RadioButton("RGB (none)", &fx->Mode, 0);
    ImGui::RadioButton("RBG", &fx->Mode, 1);
    ImGui::RadioButton("GRB", &fx->Mode, 2);
    ImGui::RadioButton("GBR", &fx->Mode, 3);
    ImGui::RadioButton("BRG", &fx->Mode, 4);
    ImGui::RadioButton("BGR", &fx->Mode, 5);

    ImGui::Checkbox("On Beat Random", &fx->OnBeatRandom);
}

void RegisterChannelShiftUI(ConfigUiRegistry& reg)
{
    reg.Register("Channel Shift", &DrawChannelShiftUI);
}
