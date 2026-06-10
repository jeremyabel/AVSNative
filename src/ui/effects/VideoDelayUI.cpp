#include "ui/ConfigUiRegistry.h"

void RegisterVideoDelayUI(ConfigUiRegistry& reg)
{
    reg.Register("Video Delay", &DrawDefault);
}
