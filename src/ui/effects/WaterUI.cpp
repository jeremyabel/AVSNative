#include "ui/ConfigUiRegistry.h"

// Water has no parameters.
void RegisterWaterUI(ConfigUiRegistry& reg)
{
    reg.Register("Water", &DrawDefault);
}
