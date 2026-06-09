#include "ui/ConfigUiRegistry.h"

void RegisterInterferencesUI(ConfigUiRegistry& reg)
{
    reg.Register("Interferences", &DrawDefault);
}
