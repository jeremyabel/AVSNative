#include "ui/ConfigUiRegistry.h"

// Brightness's params (blend select, RGB sliders, exclude color + distance) are
// covered by the auto-generated layout.
void RegisterBrightnessUI(ConfigUiRegistry& reg)
{
    reg.Register("Brightness", &DrawDefault);
}
