#include "ui/ChainPanel.h"

#include "engine/Effect.h"
#include "engine/EffectChain.h"
#include "engine/Engine.h"
#include "engine/Registry.h"
#include "ui/App.h"   // ChainNavEntry

#include <imgui/imgui.h>

static constexpr float kPanelWidth = 260.0f;

// Returns a reference to the selection variable for the current nav level.
static int32_t& CurrentSelection(std::vector<ChainNavEntry>& nav, int32_t& rootSelected)
{
    return nav.empty() ? rootSelected : nav.back().SelectedEffect;
}

void ChainPanel::Render(Engine& engine,
                        std::vector<ChainNavEntry>& nav,
                        int32_t& rootSelected,
                        EffectChain* currentChain)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, io.DisplaySize.y - 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.88f);

    if (!ImGui::Begin("Effect Chain", nullptr, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    int32_t& selectedEffect = CurrentSelection(nav, rootSelected);

    // ── Breadcrumb navigation ─────────────────────────────────────────────────
    if (!nav.empty())
    {
        if (ImGui::SmallButton("< Back"))
        {
            nav.pop_back();
            // currentChain and selectedEffect references now point at the level above;
            // caller (App::RenderUI) will re-derive them next frame.
            ImGui::End();
            return;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Root");
        for (const auto& entry : nav)
        {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
            ImGui::TextUnformatted(entry.Label.c_str());
        }
        ImGui::Separator();
    }

    // ── Effect list ───────────────────────────────────────────────────────────
    const int32_t count = currentChain->Count();

    // Clamp selection if effects were removed.
    if (selectedEffect >= count)
        selectedEffect = count - 1;

    ImGui::BeginChild("##list", ImVec2(0.0f, -64.0f), true);

    for (int32_t i = 0; i < count; ++i)
    {
        EffectEntry& entry = currentChain->GetEntry(i);
        ImGui::PushID(i);

        ImGui::Checkbox("##en", &entry.Enabled);
        ImGui::SameLine();

        bool selected = (i == selectedEffect);
        std::string name = entry.Effect->GetDescriptor().Name;

        // Reserve space on the right for: "▶" button (if EffectList) + "x" button
        bool isContainer = (entry.Effect->GetInnerChain() != nullptr);
        float rightButtons = 22.0f + (isContainer ? 26.0f : 0.0f);

        if (ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_None,
                              ImVec2(ImGui::GetContentRegionAvail().x - rightButtons, 0.0f)))
        {
            selectedEffect = i;
        }

        // Drag-to-reorder
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload("CHAIN_ITEM", &i, sizeof(i));
            ImGui::Text("Move: %s", name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CHAIN_ITEM"))
            {
                int32_t from = *static_cast<const int32_t*>(payload->Data);
                currentChain->Move(from, i);
                if (selectedEffect == from) selectedEffect = i;
            }
            ImGui::EndDragDropTarget();
        }

        // "▶" Enter button — only shown for EffectList (and any future containers)
        if (isContainer)
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.6f, 1.0f));
            if (ImGui::SmallButton(">"))
            {
                selectedEffect = i;
                ChainNavEntry navEntry;
                navEntry.Chain          = entry.Effect->GetInnerChain();
                navEntry.Label          = name;
                navEntry.SelectedEffect = -1;
                nav.push_back(std::move(navEntry));
                ImGui::PopStyleColor();
                ImGui::PopID();
                break; // nav changed; rebuild next frame
            }
            ImGui::PopStyleColor();
        }

        // "x" Remove button
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        if (ImGui::SmallButton("x"))
        {
            currentChain->Remove(i);
            if (selectedEffect == i)      selectedEffect = -1;
            else if (selectedEffect > i)  selectedEffect--;
            ImGui::PopStyleColor();
            ImGui::PopID();
            break;
        }
        ImGui::PopStyleColor();

        ImGui::PopID();
    }

    ImGui::EndChild();

    // ── Add Effect ────────────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::Spacing();

    const std::vector<std::string>& names = engine.GetRegistry().Names();
    static int32_t s_addIndex = 0;

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
            selectedEffect = currentChain->Count();
            currentChain->Add(std::move(effect));
        }
    }

    ImGui::End();
}
