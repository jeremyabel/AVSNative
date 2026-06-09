#include "ui/ConfigUiRegistry.h"

// Dynamic Movement's params (a GLSL pixel editor, three Lua editors, bool toggles,
// grid sizes, and a source-buffer select) are all covered by the auto-generated layout.
void RegisterDynamicMovementUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Movement", &DrawDefault);
}
