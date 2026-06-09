#include "ui/ConfigUiRegistry.h"

void RegisterBlurUI(ConfigUiRegistry& reg)
{
    reg.Register("Blur", &DrawDefault);
}
