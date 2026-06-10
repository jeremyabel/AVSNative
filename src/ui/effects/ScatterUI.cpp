#include "ui/ConfigUiRegistry.h"

void RegisterScatterUI(ConfigUiRegistry& reg)
{
    // Scatter has no parameters
    reg.Register("Scatter", &DrawDefault);
}
