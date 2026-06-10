#include "ui/ConfigUiRegistry.h"

void RegisterDynamicShiftUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Shift", &DrawDefault);
}
