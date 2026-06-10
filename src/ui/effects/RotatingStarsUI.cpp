#include "ui/ConfigUiRegistry.h"

void RegisterRotatingStarsUI(ConfigUiRegistry& reg)
{
    reg.Register("Rotating Stars", &DrawDefault);
}
