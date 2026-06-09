#include "ui/ConfigUiRegistry.h"

void RegisterColorReductionUI(ConfigUiRegistry& reg)
{
    reg.Register("Color Reduction", &DrawDefault);
}
