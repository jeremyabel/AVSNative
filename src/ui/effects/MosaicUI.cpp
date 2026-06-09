#include "ui/ConfigUiRegistry.h"

void RegisterMosaicUI(ConfigUiRegistry& reg)
{
    reg.Register("Mosaic", &DrawDefault);
}
