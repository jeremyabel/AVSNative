#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

void RegisterDynamicShiftUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Shift", &DrawDefault);
}
