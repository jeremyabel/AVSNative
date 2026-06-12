#include "EffectChain.h"

#include "FBOManager.h"

// Monotonic effect-id source. Starts at 1 so 0 always means "unassigned".
static uint32_t g_nextEffectId = 1;

uint32_t AllocEffectId()
{
    return g_nextEffectId++;
}

void NoteEffectId(uint32_t id)
{
    if (id >= g_nextEffectId)
        g_nextEffectId = id + 1;
}

void EffectChain::Render(RenderContext Context)
{
    for (auto& Entry : Entries)
    {
        const bool isContainer = (Entry.Effect->GetInnerChain() != nullptr);

        if (isContainer)
        {
            // Container effects (EffectList) self-allocate their view IDs from
            // NextViewId, running the inner chain first so inner views are lower
            // (execute earlier in bgfx's ascending view order) than the output blend.
            if (Entry.Enabled)
            {
                Context.InputTexture = Context.FboManager->GetCurrent().Texture;
                Context.OutputFBO    = Context.FboManager->GetNext().Fbo;
                Entry.Effect->Render(Context);
                // EffectList::Render calls FboManager->Swap() internally.
            }
            else
            {
                // Advance by the effect's full view footprint so IDs after it stay stable.
                *Context.NextViewId += Entry.Effect->ExpectedViewCount();
            }
        }
        else if (Entry.Effect->ExpectedViewCount() == 0)
        {
            // Control-only effects (SetRenderMode, Custom BPM): no views, no image
            // output. They only mutate shared frame state, so run them in place —
            // they read the current buffer as input and never swap. Nothing to
            // advance, so view IDs stay stable whether enabled or not.
            if (Entry.Enabled)
            {
                Context.InputTexture = Context.FboManager->GetCurrent().Texture;
                Entry.Effect->Render(Context);
            }
        }
        else
        {
            // Leaf effects: pre-allocate a view block. Always advance — even when
            // disabled — so subsequent effects keep stable view IDs across toggles.
            const uint8_t viewId = *Context.NextViewId;
            *Context.NextViewId += Entry.Effect->ExpectedViewCount();

            FBOSlot& NextSlot = Context.FboManager->GetNext();
            bgfx::setViewFrameBuffer(viewId, NextSlot.Fbo);
            bgfx::setViewRect(viewId, 0, 0, (uint16_t)Context.Width, (uint16_t)Context.Height);

            if (Entry.Enabled)
            {
                bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x000000ff);
                bgfx::touch(viewId);

                Context.InputTexture = Context.FboManager->GetCurrent().Texture;
                Context.OutputFBO    = NextSlot.Fbo;
                Context.ViewId       = viewId;

                Entry.Effect->Render(Context);
            }
            else
            {
                // Reset the clear flag so there are no phantom clears if this view
                // was previously bound to a different framebuffer.
                bgfx::setViewClear(viewId, BGFX_CLEAR_NONE);
            }
        }
    }
}

void EffectChain::Add(std::unique_ptr<Effect> Effect)
{
    Entries.push_back({ std::move(Effect), true });
}

void EffectChain::Insert(int32_t Index, EffectEntry Entry)
{
    // Assign an id to freshly created effects; cross-chain moves carry a nonzero id.
    if (Entry.Id == 0)
        Entry.Id = AllocEffectId();
    Index = std::max(0, std::min(Index, (int32_t)Entries.size()));
    Entries.insert(Entries.begin() + Index, std::move(Entry));
}

EffectEntry EffectChain::TakeOut(int32_t Index)
{
    EffectEntry entry = std::move(Entries[Index]);
    Entries.erase(Entries.begin() + Index);
    return entry;
}

void EffectChain::Clear()
{
    for (auto& Entry : Entries)
        Entry.Effect->Destroy();
    Entries.clear();
}

void EffectChain::Move(int32_t From, int32_t To)
{
    if (From < 0 || From >= (int32_t)Entries.size()) return;
    if (To   < 0 || To   >= (int32_t)Entries.size()) return;
    if (From == To) return;

    EffectEntry Entry = std::move(Entries[From]);
    Entries.erase(Entries.begin() + From);
    Entries.insert(Entries.begin() + To, std::move(Entry));
}

void EffectChain::Remove(int32_t Index)
{
    if (Index < 0 || Index >= (int32_t)Entries.size())
        return;

    Entries[Index].Effect->Destroy();
    Entries.erase(Entries.begin() + Index);
}

int32_t EffectChain::Count() const
{
    return (int32_t)Entries.size();
}

EffectEntry& EffectChain::GetEntry(int32_t Index)
{
    return Entries[Index];
}

const EffectEntry& EffectChain::GetEntry(int32_t Index) const
{
    return Entries[Index];
}
