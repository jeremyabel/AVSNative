#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

void RegisterColorModifierUI(ConfigUiRegistry& reg)
{
    reg.Register("Color Modifier", &DrawDefault);
}
