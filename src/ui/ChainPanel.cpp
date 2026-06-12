#include "ui/ChainPanel.h"

#include "engine/Effect.h"
#include "engine/EffectChain.h"
#include "engine/Engine.h"
#include "engine/Registry.h"

#include <imgui.h>
#include <algorithm>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct ChainDragPayload
{
    EffectChain* SrcChain;
    int32_t      SrcIdx;
};

// Returns true if 'needle' is reachable from 'root' (directly or via containers).
static bool IsChainReachable(EffectChain* root, EffectChain* needle)
{
    if (!root || !needle) return false;
    if (root == needle)   return true;
    for (int32_t i = 0; i < root->Count(); ++i)
    {
        EffectChain* inner = root->GetEntry(i).Effect->GetInnerChain();
        if (inner && IsChainReachable(inner, needle))
            return true;
    }
    return false;
}

// Apply a drag-drop move. Same-chain → Move(); cross-chain → TakeOut + Insert.
// Returns the new index of the moved item in dstChain.
static int32_t ApplyMove(const ChainDragPayload& src, EffectChain* dst, int32_t dstIdx)
{
    if (src.SrcChain == dst)
    {
        dst->Move(src.SrcIdx, dstIdx);
        return dstIdx;
    }
    EffectEntry moved = src.SrcChain->TakeOut(src.SrcIdx);
    dst->Insert(dstIdx, std::move(moved));
    return dstIdx;
}

// Update (chain, idx) selection after a same-chain Move(from, to).
static void AdjustSelectionAfterMove(EffectChain*& sc, int32_t& si,
                                     EffectChain* chain,
                                     int32_t from, int32_t to)
{
    if (sc != chain) return;
    if      (si == from)                        si = to;
    else if (from < to && si > from && si <= to) si--;
    else if (from > to && si >= to && si < from) si++;
}

// Update (chain, idx) selection after a TakeOut from srcChain at srcIdx.
static void AdjustSelectionAfterTakeOut(EffectChain*& sc, int32_t& si,
                                        EffectChain* srcChain, int32_t srcIdx)
{
    if (sc == srcChain && si > srcIdx) si--;
}

// True if moving the dragged effect into 'target' would nest a container inside
// its own subtree (or itself) — which would create a cycle and recurse forever.
static bool WouldCreateCycle(const ChainDragPayload& pl, EffectChain* target)
{
    if (pl.SrcIdx < 0 || pl.SrcIdx >= pl.SrcChain->Count()) return false;
    EffectChain* inner = pl.SrcChain->GetEntry(pl.SrcIdx).Effect->GetInnerChain();
    return inner && IsChainReachable(inner, target);
}

// A thin drop zone representing insertion index 'idx' (0..Count) in 'chain'.
// Drawing an insertion line while hovered. Returns true if a drop mutated a chain.
static bool RenderGap(EffectChain& chain, int32_t idx,
                      EffectChain*& selectedChain, int32_t& selectedIdx)
{
    ImGui::PushID(idx);
    // Keep the hit zone thin without inflating the list's vertical rhythm.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##gap", ImVec2(w > 1.0f ? w : 1.0f, 5.0f));
    ImGui::PopStyleVar();

    bool mutated = false;
    if (ImGui::BeginDragDropTarget())
    {
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        const float  y = (a.y + b.y) * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(a.x, y), ImVec2(b.x, y),
            ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);

        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(
                "CHAIN_ITEM", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
        {
            const auto& pl = *static_cast<const ChainDragPayload*>(p->Data);
            if (!WouldCreateCycle(pl, &chain))
            {
                const bool selWasSrc = (selectedChain == pl.SrcChain &&
                                        selectedIdx   == pl.SrcIdx);
                int32_t landed;
                if (pl.SrcChain == &chain)
                {
                    // Same chain: convert the gap index to Move()'s post-erase target.
                    const int32_t to = (pl.SrcIdx < idx) ? idx - 1 : idx;
                    AdjustSelectionAfterMove(selectedChain, selectedIdx, &chain, pl.SrcIdx, to);
                    chain.Move(pl.SrcIdx, to);
                    landed = to;
                }
                else
                {
                    AdjustSelectionAfterTakeOut(selectedChain, selectedIdx, pl.SrcChain, pl.SrcIdx);
                    if (selectedChain == &chain && selectedIdx >= idx)
                        selectedIdx++;
                    EffectEntry moved = pl.SrcChain->TakeOut(pl.SrcIdx);
                    chain.Insert(idx, std::move(moved));
                    landed = idx;
                }
                if (selWasSrc) { selectedChain = &chain; selectedIdx = landed; }
                mutated = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::PopID();
    return mutated;
}

// ---------------------------------------------------------------------------
// Recursive item renderer
// Returns true when a structural mutation happened and the caller should break.
// ---------------------------------------------------------------------------

static bool RenderChainItems(EffectChain& chain,
                             EffectChain*& selectedChain,
                             int32_t& selectedIdx)
{
    const float btnW    = ImGui::CalcTextSize("x").x
                          + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float lockW   = ImGui::CalcTextSize("L").x
                          + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    // Width reserved at the right edge for the two row buttons (L then x) plus the
    // gap between them.
    const float btnsW   = lockW + spacing + btnW;

    // Draws the amber "L" lock toggle. Flips entry.Locked (assigning an id on first
    // lock). Pure UI state — no structural change, so callers don't early-return.
    auto LockButton = [](EffectEntry& e)
    {
        const bool on = e.Locked;
        if (on)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.6f, 0.1f, 1.0f));
        if (ImGui::SmallButton("L"))
        {
            e.Locked = !e.Locked;
            if (e.Locked && e.Id == 0)
                e.Id = AllocEffectId();
        }
        if (on)
            ImGui::PopStyleColor();
    };

    // Insertion gap above the first item (targets index 0 of this chain).
    if (RenderGap(chain, 0, selectedChain, selectedIdx))
        return true;

    for (int32_t i = 0; i < chain.Count(); ++i)
    {
        EffectEntry& entry      = chain.GetEntry(i);
        const bool   isContainer = (entry.Effect->GetInnerChain() != nullptr);
        const bool   selected    = (selectedChain == &chain && selectedIdx == i);
        const std::string name   = entry.Effect->Name();

        ImGui::PushID(entry.Effect.get());

        bool doRemove = false;

        // ── Checkbox ─────────────────────────────────────────────────────────
        ImGui::Checkbox("##en", &entry.Enabled);
        ImGui::SameLine();

        if (isContainer)
        {
            // ── Tree-node for container effects (EffectList etc.) ─────────────
            const ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_OpenOnArrow    |
                ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_AllowOverlap   |
                (selected ? ImGuiTreeNodeFlags_Selected : 0);

            bool open = ImGui::TreeNodeEx("##tn", flags, "%s", name.c_str());

            // Click on the label area (not the arrow) selects the effect.
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                selectedChain = &chain;
                selectedIdx   = i;
            }

            // Drag source: drag the EffectList itself.
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ChainDragPayload pl{ &chain, i };
                ImGui::SetDragDropPayload("CHAIN_ITEM", &pl, sizeof(pl));
                ImGui::Text("Move: %s", name.c_str());
                ImGui::EndDragDropSource();
            }

            // Drop ON the EffectList header → append to its inner chain.
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CHAIN_ITEM"))
                {
                    const auto& pl    = *static_cast<const ChainDragPayload*>(p->Data);
                    EffectChain* inner = entry.Effect->GetInnerChain();
                    // Don't drop onto itself, and don't nest a container in its own subtree.
                    if ((pl.SrcChain != &chain || pl.SrcIdx != i) &&
                        !WouldCreateCycle(pl, inner))
                    {
                        bool selWasSrc = (selectedChain == pl.SrcChain &&
                                          selectedIdx   == pl.SrcIdx);
                        int32_t insertAt = inner->Count();
                        // If moving within the same chain and src comes before this
                        // node, the outer chain's indices don't shift for the inner.
                        AdjustSelectionAfterTakeOut(selectedChain, selectedIdx,
                                                    pl.SrcChain, pl.SrcIdx);
                        ApplyMove(pl, inner, insertAt);
                        if (selWasSrc)
                        {
                            selectedChain = inner;
                            selectedIdx   = inner->Count() - 1;
                        }
                    }
                    ImGui::EndDragDropTarget();
                    // TreeNodeEx pushed the tree stack when open — balance it
                    // before the early return or the ImGui stack desyncs/crashes.
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return true;
                }
                ImGui::EndDragDropTarget();
            }

            // "L" lock toggle + "x" remove, overlaid at the right edge of the header.
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - btnsW);
            LockButton(entry);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            doRemove = ImGui::SmallButton("x");
            ImGui::PopStyleColor();

            if (open)
            {
                EffectChain* inner = entry.Effect->GetInnerChain();
                bool mutated = RenderChainItems(*inner, selectedChain, selectedIdx);
                ImGui::TreePop();
                if (mutated) { ImGui::PopID(); return true; }
            }
        }
        else
        {
            // ── Selectable for leaf effects ───────────────────────────────────
            const float labelW = ImGui::GetContentRegionAvail().x - btnsW - spacing;
            if (ImGui::Selectable(name.c_str(), selected,
                                  ImGuiSelectableFlags_None,
                                  ImVec2(labelW, 0.0f)))
            {
                selectedChain = &chain;
                selectedIdx   = i;
            }

            // Drag source. (Reordering/insertion is handled by the gap targets;
            // leaf items are not themselves drop targets.)
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ChainDragPayload pl{ &chain, i };
                ImGui::SetDragDropPayload("CHAIN_ITEM", &pl, sizeof(pl));
                ImGui::Text("Move: %s", name.c_str());
                ImGui::EndDragDropSource();
            }

            // "L" lock toggle + "x" remove.
            ImGui::SameLine();
            LockButton(entry);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            doRemove = ImGui::SmallButton("x");
            ImGui::PopStyleColor();
        }

        // ── Remove (deferred until after TreePop so ImGui stack is balanced) ─
        if (doRemove)
        {
            // Clear selection if it lives inside the subtree being removed.
            if (isContainer &&
                IsChainReachable(entry.Effect->GetInnerChain(), selectedChain))
            {
                selectedChain = nullptr;
                selectedIdx   = -1;
            }
            if (selectedChain == &chain)
            {
                if      (selectedIdx == i) { selectedChain = nullptr; selectedIdx = -1; }
                else if (selectedIdx  > i) selectedIdx--;
            }
            chain.Remove(i);
            ImGui::PopID();
            return true;
        }

        ImGui::PopID();

        // Insertion gap below this item (targets index i+1 of this chain).
        if (RenderGap(chain, i + 1, selectedChain, selectedIdx))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

void ChainPanel::Render(Engine& engine,
                        EffectChain& rootChain,
                        EffectChain*& selectedChain,
                        int32_t& selectedIdx)
{
    if (!ImGui::Begin("Effect Chain", nullptr, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    // Safety: if selectedChain no longer exists in the tree, clear selection.
    if (selectedChain && !IsChainReachable(&rootChain, selectedChain))
    {
        selectedChain = nullptr;
        selectedIdx   = -1;
    }
    // Clamp index within the selected chain.
    if (selectedChain && selectedIdx >= selectedChain->Count())
        selectedIdx = selectedChain->Count() - 1;

    ImGui::BeginChild("##list", ImVec2(0.0f, -64.0f), true);
    RenderChainItems(rootChain, selectedChain, selectedIdx);
    ImGui::EndChild();

    // ── Add Effect ────────────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::Spacing();

    const std::vector<std::string>& names = engine.GetRegistry().Names();
    static int32_t s_addIndex = 0;
    if (s_addIndex >= (int32_t)names.size()) s_addIndex = 0;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
    if (ImGui::BeginCombo("##add", names.empty() ? "(none)" : names[s_addIndex].c_str()))
    {
        for (int32_t i = 0; i < (int32_t)names.size(); ++i)
        {
            bool picked = (i == s_addIndex);
            if (ImGui::Selectable(names[i].c_str(), picked))
                s_addIndex = i;
            if (picked)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Add") && !names.empty())
    {
        auto effect = engine.GetRegistry().Create(names[s_addIndex]);
        if (effect)
        {
            effect->Init();

            // Insert after the currently selected item in its chain,
            // or append to the root chain when nothing is selected.
            EffectChain* target = selectedChain ? selectedChain : &rootChain;
            int32_t insertAt = (selectedChain && selectedIdx >= 0)
                ? std::min(selectedIdx + 1, target->Count())
                : target->Count();

            target->Insert(insertAt, { std::move(effect), true });
            selectedChain = target;
            selectedIdx   = insertAt;
        }
    }

    ImGui::End();
}
