#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

void RegisterRotatingStarsUI(ConfigUiRegistry& reg)
{
    reg.Register("Rotating Stars", &DrawDefault);
}
