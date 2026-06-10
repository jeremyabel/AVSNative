#include "ui/ConfigUiRegistry.h"

void RegisterDynamicDistanceModifierUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Distance Modifier", &DrawDefault);
}
