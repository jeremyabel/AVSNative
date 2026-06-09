#include "ui/ConfigUiRegistry.h"

void RegisterBassSpinUI(ConfigUiRegistry& reg)
{
    reg.Register("Bass Spin", &DrawDefault);
}
