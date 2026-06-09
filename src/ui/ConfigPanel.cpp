#include "ui/ConfigPanel.h"
#include "ui/ConfigUi.h"
#include "ui/ConfigUiRegistry.h"

#include "engine/Engine.h"
#include "engine/Effect.h"

#include <imgui/imgui.h>

static Effect* s_lastEffect = nullptr;

static constexpr float kPanelWidth = 300.0f;

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

    ImGuiIO&    io     = ImGui::GetIO();
    const float panelX = io.DisplaySize.x - kPanelWidth - 10.0f;
    ImGui::SetNextWindowPos(ImVec2(panelX, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, io.DisplaySize.y - 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.88f);

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

    const std::string name = effect->GetDescriptor().Name;

    ImGui::TextUnformatted(name.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    if (const EffectUiDraw* draw = UiRegistry().Find(name))
        (*draw)(effect);
    else
        DrawDefault(effect);

    ImGui::End();
}
