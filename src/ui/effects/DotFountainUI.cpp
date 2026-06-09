#include "ui/ConfigUiRegistry.h"

// Dot Fountain's params (5 colors + two sliders) are covered by the auto-generated layout.
void RegisterDotFountainUI(ConfigUiRegistry& reg)
{
    reg.Register("Dot Fountain", &DrawDefault);
}
