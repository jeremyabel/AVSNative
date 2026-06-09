#include "ui/ConfigUiRegistry.h"

void RegisterColorFadeUI(ConfigUiRegistry& reg)
{
    reg.Register("Colorfade", &DrawDefault);
}
