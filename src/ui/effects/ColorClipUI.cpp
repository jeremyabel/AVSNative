#include "ui/ConfigUiRegistry.h"

void RegisterColorClipUI(ConfigUiRegistry& reg)
{
    reg.Register("Color Clip", &DrawDefault);
}
