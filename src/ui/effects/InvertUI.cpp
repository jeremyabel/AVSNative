#include "ui/ConfigUiRegistry.h"

// Invert has no parameters.
void RegisterInvertUI(ConfigUiRegistry& reg)
{
    reg.Register("Invert", &DrawDefault);
}
