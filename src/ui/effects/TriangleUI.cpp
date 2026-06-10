#include "ui/ConfigUiRegistry.h"

void RegisterTriangleUI(ConfigUiRegistry& reg)
{
    reg.Register("Triangle", &DrawDefault);
}
