#pragma once

#include <cstdint>

class Engine;
class EffectChain;

class ChainPanel
{
public:
    // Renders the full effect-chain tree (root + all nested EffectList children).
    // selectedChain / selectedIdx identify the currently selected effect; both are
    // written by this function as the user clicks or drags.  selectedChain is nullptr
    // when nothing is selected.
    static void Render(Engine& engine,
                       EffectChain& rootChain,
                       EffectChain*& selectedChain,
                       int32_t& selectedIdx);
};
