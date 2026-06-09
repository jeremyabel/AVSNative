#include "ui/ConfigUiRegistry.h"

void RegisterMultiplierUI(ConfigUiRegistry& reg)
{
    reg.Register("Multiplier", &DrawDefault);
}
