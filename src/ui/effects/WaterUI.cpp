#include "ui/ConfigUiRegistry.h"

void RegisterWaterUI(ConfigUiRegistry& reg)
{
    // Water has no parameters
    reg.Register("Water", &DrawDefault);
}
