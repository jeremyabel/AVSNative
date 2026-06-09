#include "ui/ConfigUiRegistry.h"

void RegisterMirrorUI(ConfigUiRegistry& reg)
{
    reg.Register("Mirror", &DrawDefault);
}
