#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

void RegisterNormalizeUI(ConfigUiRegistry& reg)
{
    reg.Register("Normalize", &DrawDefault);
}
