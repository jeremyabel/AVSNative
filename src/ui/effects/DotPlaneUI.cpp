#include "ui/ConfigUiRegistry.h"

void RegisterDotPlaneUI(ConfigUiRegistry& reg)
{
    reg.Register("Dot Plane", &DrawDefault);
}
