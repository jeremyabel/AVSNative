#include "ui/ConfigUiRegistry.h"

void RegisterBlitEffectUI(ConfigUiRegistry& reg)
{
    reg.Register("Blit", &DrawDefault);
}
