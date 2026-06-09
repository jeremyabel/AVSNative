#include "ui/ConfigUiRegistry.h"

// Video Delay's params (two bools + a delay slider) are all covered by the
// auto-generated layout.
void RegisterVideoDelayUI(ConfigUiRegistry& reg)
{
    reg.Register("Video Delay", &DrawDefault);
}
