#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

void RegisterRingUI(ConfigUiRegistry& reg)
{
    reg.Register("Ring", &DrawDefault);
}
