#include "ui/ConfigUiRegistry.h"

// Timescope's params (source select, color, blend select, bands slider) are covered
// by the auto-generated layout.
void RegisterTimescopeUI(ConfigUiRegistry& reg)
{
    reg.Register("Timescope", &DrawDefault);
}
