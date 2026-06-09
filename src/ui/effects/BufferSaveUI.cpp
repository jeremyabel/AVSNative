#include "ui/ConfigUiRegistry.h"

void RegisterBufferSaveUI(ConfigUiRegistry& reg)
{
    reg.Register("Buffer Save", &DrawDefault);
}
