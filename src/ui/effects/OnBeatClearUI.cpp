#include "ui/ConfigUiRegistry.h"

void RegisterOnBeatClearUI(ConfigUiRegistry& reg)
{
    reg.Register("OnBeat Clear", &DrawDefault);
}
