#include "ui/ConfigUiRegistry.h"

// Scatter has no parameters.
void RegisterScatterUI(ConfigUiRegistry& reg)
{
    reg.Register("Scatter", &DrawDefault);
}
