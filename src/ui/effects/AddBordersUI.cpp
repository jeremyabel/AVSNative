#include "ui/ConfigUiRegistry.h"

void RegisterAddBordersUI(ConfigUiRegistry& reg)
{
    reg.Register("Add Borders", &DrawDefault);
}
