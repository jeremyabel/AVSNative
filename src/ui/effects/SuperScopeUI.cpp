#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/SuperScope.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

static void DrawSuperScopeUI(Effect* base)
{
    auto* fx = static_cast<SuperScope*>(base);

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("sscope.initCode", fx->InitCode, ConfigUi::Lang::Lua))
            fx->Recompile();
        ScriptError(fx, SuperScope::kInitCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("sscope.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
            fx->Recompile();
        ScriptError(fx, SuperScope::kFrameCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("sscope.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
            fx->Recompile();
        ScriptError(fx, SuperScope::kBeatCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Point", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("sscope.pointCode", fx->PointCode, ConfigUi::Lang::Lua))
            fx->Recompile();
        ScriptError(fx, SuperScope::kPointCode);
        ImGui::TreePop();
    }

    ImGui::TextUnformatted("Color");
    ConfigUi::ColorsEdit("##colors", fx->Colors);

    static const char* kSources[] = { "Waveform", "Spectrum" };
    ImGui::TextUnformatted("Source");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##audioSource", &fx->AudioSource, kSources, IM_ARRAYSIZE(kSources));

    static const char* kChannels[] = { "Center", "Left", "Right" };
    ImGui::TextUnformatted("Channel");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##audioChannel", &fx->AudioChannel, kChannels, IM_ARRAYSIZE(kChannels));

    static const char* kDraws[] = { "Dots", "Lines" };
    ImGui::TextUnformatted("Draw");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##drawMode", &fx->DrawMode, kDraws, IM_ARRAYSIZE(kDraws));
}

void RegisterSuperScopeUI(ConfigUiRegistry& reg)
{
    reg.Register("Super Scope", &DrawSuperScopeUI);
}
