#include "ui/ConfigUiRegistry.h"

void RegisterDynamicMovementUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Movement", &DrawDefault);
}
