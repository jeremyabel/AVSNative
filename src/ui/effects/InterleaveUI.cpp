#include "ui/ConfigUiRegistry.h"

void RegisterInterleaveUI(ConfigUiRegistry& reg)
{
    reg.Register("Interleave", &DrawDefault);
}
