#include "ui/ConfigUiRegistry.h"

void RegisterNormalizeUI(ConfigUiRegistry& reg)
{
    reg.Register("Normalize", &DrawDefault);
}
