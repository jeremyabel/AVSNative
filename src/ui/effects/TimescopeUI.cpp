#include "ui/ConfigUiRegistry.h"

void RegisterTimescopeUI(ConfigUiRegistry& reg)
{
    reg.Register("Timescope", &DrawDefault);
}
