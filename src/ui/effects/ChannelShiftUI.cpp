#include "ui/ConfigUiRegistry.h"

void RegisterChannelShiftUI(ConfigUiRegistry& reg)
{
    reg.Register("Channel Shift", &DrawDefault);
}
