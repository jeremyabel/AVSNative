#include "ui/ConfigUiRegistry.h"

void RegisterWaterBumpUI(ConfigUiRegistry& reg)
{
    reg.Register("Water Bump", &DrawDefault);
}
