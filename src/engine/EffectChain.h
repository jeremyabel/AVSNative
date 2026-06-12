#pragma once

#include "Effect.h"

#include <cstdint>
#include <memory>
#include <vector>

struct EffectEntry
{
    std::unique_ptr<Effect> Effect;
    bool Enabled = true;
    // Open the effect's properties in a dedicated dockable panel that stays up
    // regardless of selection (see ConfigPanel::RenderLockedPanels). Serialized.
    bool Locked = false;
    // Stable per-effect identity (0 = unassigned). Travels with the effect through
    // reorder/drag because it lives on the entry, and is serialized so a locked
    // panel's docked position (keyed by this id in imgui.ini) survives reloads.
    uint32_t Id = 0;
};

// Allocate a fresh, process-unique effect id (always non-zero). Used when a new
// effect is inserted or first locked.
uint32_t AllocEffectId();
// Ensure the allocator never re-issues a loaded id (call on preset load).
void NoteEffectId(uint32_t id);

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
