#include "ui/ConfigUiRegistry.h"

void RegisterDotGridUI(ConfigUiRegistry& reg)
{
    reg.Register("Dot Grid", &DrawDefault);
}
