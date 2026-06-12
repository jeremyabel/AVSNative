#include "ui/ConfigPanel.h"
#include "ui/ConfigUi.h"
#include "ui/ConfigUiRegistry.h"

#include "engine/Engine.h"
#include "engine/Effect.h"

#include <imgui.h>

static Effect* s_lastEffect = nullptr;

static ConfigUiRegistry& UiRegistry()
{
    static ConfigUiRegistry reg = [] {
        ConfigUiRegistry r;
        RegisterAllEffectUis(r);
        return r;
    }();
    return reg;
}

void ConfigPanel::Render(Engine& engine, Effect* effect)
{
    (void)engine;

    // Dockable panel — position/size come from the dockspace (or imgui.ini).
    if (!ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    if (!effect)
    {
        ImGui::TextDisabled("No effect selected.");
        s_lastEffect = nullptr;
        ConfigUi::ResetEditors();
        ImGui::End();
        return;
    }

    if (effect != s_lastEffect)
    {
        ConfigUi::ResetEditors();
        s_lastEffect = effect;
    }

    const std::string name = effect->Name();

    ImGui::TextUnformatted(name.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    if (const EffectUiDraw* draw = UiRegistry().Find(name))
        (*draw)(effect);
    else
        ImGui::TextDisabled("No UI registered.");

    ImGui::End();
}
