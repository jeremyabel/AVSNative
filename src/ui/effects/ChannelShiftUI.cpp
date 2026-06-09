#include "ui/ConfigUiRegistry.h"

// Channel Shift uses the auto-generated layout. Customize here if desired.
void RegisterChannelShiftUI(ConfigUiRegistry& reg)
{
    reg.Register("Channel Shift", &DrawDefault);
}
