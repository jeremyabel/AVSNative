#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

void RegisterOscilloscopeStarUI(ConfigUiRegistry& reg)
{
    reg.Register("Oscilloscope Star", &DrawDefault);
}
