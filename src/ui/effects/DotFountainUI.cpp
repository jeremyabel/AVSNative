#include "ui/ConfigUiRegistry.h"

void RegisterDotFountainUI(ConfigUiRegistry& reg)
{
    reg.Register("Dot Fountain", &DrawDefault);
}
