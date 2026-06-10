#include "ui/ConfigUiRegistry.h"

void RegisterInvertUI(ConfigUiRegistry& reg)
{
    // Invert has no parameters
    reg.Register("Invert", &DrawDefault);
}
