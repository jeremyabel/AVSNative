#include "ui/ConfigUiRegistry.h"

void RegisterRotoBlitterUI(ConfigUiRegistry& reg)
{
    reg.Register("Roto Blitter", &DrawDefault);
}
