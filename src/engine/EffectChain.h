#pragma once

#include "Effect.h"

#include <memory>
#include <vector>

struct EffectEntry
{
    std::unique_ptr<Effect> Effect;
    bool Enabled = true;
};

class EffectChain
{
public:

    // Render iterates enabled effects. The chain owns view ID allocation:
    // effect 0 gets ViewId 0, effect 1 gets ViewId 1, etc.
    // Each effect reads Context.InputTexture, draws to Context.ViewId (bound to
    // Context.OutputFBO by the chain), then calls FboManager->Swap().
    void Render(RenderContext Context);

    void Add(std::unique_ptr<Effect> Effect);
    // Insert an EffectEntry at the given index (shifting later entries down).
    // Index is clamped to [0, Count()], so Insert(Count(), ...) is equivalent to Add.
    void Insert(int32_t Index, EffectEntry Entry);
    // Remove the entry at Index and return it WITHOUT calling Destroy().
    // Used for cross-chain drag-and-drop; caller takes ownership.
    EffectEntry TakeOut(int32_t Index);
    void Remove(int32_t Index);
    void Move(int32_t From, int32_t To);
    // Calls Destroy() on every effect and clears the list.
    // Must be called while bgfx is still alive.
    void Clear();

    int32_t Count() const;
    EffectEntry&       GetEntry(int32_t Index);
    const EffectEntry& GetEntry(int32_t Index) const;

private:

    std::vector<EffectEntry> Entries;
};
