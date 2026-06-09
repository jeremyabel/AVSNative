#include "ui/ConfigUiRegistry.h"

void RegisterFastBrightnessUI(ConfigUiRegistry& reg)
{
    reg.Register("Fast Brightness", &DrawDefault);
}
