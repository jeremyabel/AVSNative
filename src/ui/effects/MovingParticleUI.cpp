#include "ui/ConfigUiRegistry.h"

void RegisterMovingParticleUI(ConfigUiRegistry& reg)
{
    reg.Register("MovingParticle", &DrawDefault);
}
