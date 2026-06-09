#pragma once

#include <cstdint>
#include <vector>

class Engine;
class EffectChain;
struct ChainNavEntry;

class ChainPanel
{
public:
    // Renders the effect chain list.
    // nav:            navigation stack managed by App (may be pushed/popped here)
    // rootSelected:   selection index in the root chain (App owns this)
    // currentChain:   the chain currently being displayed (derived from nav by App)
    static void Render(Engine& engine,
                       std::vector<ChainNavEntry>& nav,
                       int32_t& rootSelected,
                       EffectChain* currentChain);
};
