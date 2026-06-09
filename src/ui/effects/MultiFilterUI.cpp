#include "ui/ConfigUiRegistry.h"

void RegisterMultiFilterUI(ConfigUiRegistry& reg)
{
    reg.Register("Multi Filter", &DrawDefault);
}
