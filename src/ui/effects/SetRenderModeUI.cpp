#include "ui/ConfigUiRegistry.h"

void RegisterSetRenderModeUI(ConfigUiRegistry& reg)
{
    reg.Register("Set Render Mode", &DrawDefault);
}
