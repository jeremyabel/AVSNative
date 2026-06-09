#include "ui/ConfigUiRegistry.h"

void RegisterUniqueToneUI(ConfigUiRegistry& reg)
{
    reg.Register("Unique Tone", &DrawDefault);
}
