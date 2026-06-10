#include "ui/ConfigUiRegistry.h"

void RegisterRingUI(ConfigUiRegistry& reg)
{
    reg.Register("Ring", &DrawDefault);
}
