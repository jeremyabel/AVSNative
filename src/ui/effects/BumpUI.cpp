#include "ui/ConfigUiRegistry.h"

void RegisterBumpUI(ConfigUiRegistry& reg)
{
    reg.Register("Bump", &DrawDefault);
}
