#include "ui/ConfigUiRegistry.h"

void RegisterColorModifierUI(ConfigUiRegistry& reg)
{
    reg.Register("Color Modifier", &DrawDefault);
}
