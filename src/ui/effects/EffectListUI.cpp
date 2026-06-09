#include "ui/ConfigUiRegistry.h"

// Effect List's own parameters use the auto-generated layout. The inner effect
// chain is edited via the main ChainPanel, not here.
void RegisterEffectListUI(ConfigUiRegistry& reg)
{
    reg.Register("Effect List", &DrawDefault);
}
