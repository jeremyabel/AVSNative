#include "ui/ConfigUiRegistry.h"

void RegisterEffectListUI(ConfigUiRegistry& reg)
{
    reg.Register("Effect List", &DrawDefault);
}
