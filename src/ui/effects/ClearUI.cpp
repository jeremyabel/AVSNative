#include "ui/ConfigUiRegistry.h"

void RegisterClearUI(ConfigUiRegistry& reg)
{
    reg.Register("Clear", &DrawDefault);
}
