#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

// DrawDefault renders the Bool checkboxes + Glsl/Lua code editors (with per-block
// error display) from the reflected Fields() table — no bespoke layout needed.
void RegisterDynamicDistanceModifierUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Distance Modifier", &DrawDefault);
}
