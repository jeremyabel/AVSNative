#include "ui/ConfigUiRegistry.h"

void RegisterStarfieldUI(ConfigUiRegistry& reg)
{
    reg.Register("Starfield", &DrawDefault);
}
