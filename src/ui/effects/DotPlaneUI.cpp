#include "ui/ConfigUiRegistry.h"

// Dot Plane's params (two sliders + 5 colors) are covered by the auto-generated layout.
void RegisterDotPlaneUI(ConfigUiRegistry& reg)
{
    reg.Register("Dot Plane", &DrawDefault);
}
