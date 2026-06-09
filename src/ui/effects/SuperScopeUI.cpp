#include "ui/ConfigUiRegistry.h"

// Super Scope's params (Lua editors, color list, selects) are all handled by the
// auto-generated layout. Customize here if a bespoke arrangement is wanted.
void RegisterSuperScopeUI(ConfigUiRegistry& reg)
{
    reg.Register("Super Scope", &DrawDefault);
}
