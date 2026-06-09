#include "ui/ConfigUiRegistry.h"

// Triangle's params are four Lua code editors — the auto-generated layout (which
// renders Lua params with the code editor + error display) covers them.
void RegisterTriangleUI(ConfigUiRegistry& reg)
{
    reg.Register("Triangle", &DrawDefault);
}
