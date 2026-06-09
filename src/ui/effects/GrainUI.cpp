#include "ui/ConfigUiRegistry.h"

void RegisterGrainUI(ConfigUiRegistry& reg)
{
    reg.Register("Grain", &DrawDefault);
}
