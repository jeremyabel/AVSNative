#include "ui/ConfigUiRegistry.h"

void RegisterOscilloscopeStarUI(ConfigUiRegistry& reg)
{
    reg.Register("Oscilloscope Star", &DrawDefault);
}
