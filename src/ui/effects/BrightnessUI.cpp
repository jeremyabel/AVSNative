#include "ui/ConfigUiRegistry.h"

void RegisterBrightnessUI(ConfigUiRegistry& reg)
{
    reg.Register("Brightness", &DrawDefault);
}
