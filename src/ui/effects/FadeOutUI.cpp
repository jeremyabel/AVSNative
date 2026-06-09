#include "ui/ConfigUiRegistry.h"

void RegisterFadeOutUI(ConfigUiRegistry& reg)
{
    reg.Register("FadeOut", &DrawDefault);
}
