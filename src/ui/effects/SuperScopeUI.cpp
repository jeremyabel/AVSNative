#include "ui/ConfigUiRegistry.h"

void RegisterSuperScopeUI(ConfigUiRegistry& reg)
{
    reg.Register("Super Scope", &DrawDefault);
}
