#include "ui/ConfigPanel.h"
#include "ui/ConfigUi.h"
#include "ui/ConfigUiRegistry.h"

#include "engine/Engine.h"
#include "engine/Effect.h"
#include "engine/EffectChain.h"

#include <imgui.h>

#include <string>

static ConfigUiRegistry& UiRegistry()
{
    static ConfigUiRegistry reg = [] {
        ConfigUiRegistry r;
        RegisterAllEffectUis(r);
        return r;
    }();
    return reg;
}

// Dock node of the shared "Properties" window, captured each frame so a freshly
// locked panel can default to appearing as a tab beside it.
static ImGuiID s_propDockId = 0;

// Draws an effect's name header + its registered config body, scoped so its code
// editors are independent from any other panel showing a different effect instance.
static void DrawBody(Effect* effect)
{
    ConfigUi::SetEditorScope(effect);

    const std::string name = effect->Name();
    ImGui::TextUnformatted(name.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    if (const EffectUiDraw* draw = UiRegistry().Find(name))
        (*draw)(effect);
    else
        ImGui::TextDisabled("No UI registered.");

    ConfigUi::SetEditorScope(nullptr);
}

void ConfigPanel::Render(Engine& engine, EffectEntry* selected)
{
    (void)engine;

    // Dockable panel — position/size come from the dockspace (or imgui.ini).
    if (!ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    s_propDockId = ImGui::GetWindowDockID();

    if (!selected)
        ImGui::TextDisabled("No effect selected.");
    else if (selected->Locked)
        ImGui::TextDisabled("This effect is open in its own locked panel.");
    else
        DrawBody(selected->Effect.get());

    ImGui::End();
}

// Recursively render a locked panel for every locked entry in the tree.
static void RenderLockedRecursive(EffectChain& chain)
{
    for (int32_t i = 0; i < chain.Count(); ++i)
    {
        EffectEntry& entry = chain.GetEntry(i);

        if (entry.Locked)
        {
            // Default into the Properties dock node on first appearance; afterwards
            // the user can move it and imgui.ini remembers it by the stable id.
            if (s_propDockId != 0)
                ImGui::SetNextWindowDockID(s_propDockId, ImGuiCond_FirstUseEver);

            const std::string title =
                entry.Effect->Name() + "###avsprop" + std::to_string(entry.Id);

            bool open = true;
            if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_None))
                DrawBody(entry.Effect.get());
            ImGui::End();

            if (!open)
                entry.Locked = false;
        }

        if (EffectChain* inner = entry.Effect->GetInnerChain())
            RenderLockedRecursive(*inner);
    }
}

void ConfigPanel::RenderLockedPanels(Engine& engine, EffectChain& root)
{
    (void)engine;
    RenderLockedRecursive(root);
}
