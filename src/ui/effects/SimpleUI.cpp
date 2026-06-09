#include "ui/ConfigUiRegistry.h"

void RegisterSimpleUI(ConfigUiRegistry& reg)
{
    reg.Register("Simple", &DrawDefault);
}
