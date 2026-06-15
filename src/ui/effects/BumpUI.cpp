#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Bump.h"

#include <imgui.h>

static void DrawBumpUI(Effect* base)
{
    auto* fx = static_cast<Bump*>(base);

    ImGui::Text("Depth");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##depth", &fx->Depth, 1, 100);

    ImGui::Checkbox("On Beat", &fx->EnableOnBeatChange);
    if (fx->EnableOnBeatChange)
    {
        ImGui::Text("On Beat Duration");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##onBeatDuration", &fx->OnBeatDuration, 0, 100);

        ImGui::Text("On Beat Depth");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##onBeatDepth", &fx->OnBeatDepth, 1, 100);
    }

    static const char* kBlends[] = { "Replace", "Additive", "50/50" };
    ImGui::Text("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &fx->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::Checkbox("Show Light Pos", &fx->ShowLightPos);
    ImGui::Checkbox("Invert Depth", &fx->InvertDepth);

    ImGui::Separator();

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("bump.initCode", fx->InitCode, ConfigUi::Lang::Lua))
            fx->RecompileCode();
        ConfigUi::ScriptError(fx, Bump::NAME_InitCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("bump.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
            fx->RecompileCode();
        ConfigUi::ScriptError(fx, Bump::NAME_FrameCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("bump.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
            fx->RecompileCode();
        ConfigUi::ScriptError(fx, Bump::NAME_BeatCode);
        ImGui::TreePop();
    }
}

void RegisterBumpUI(ConfigUiRegistry& reg)
{
    reg.Register("Bump", &DrawBumpUI);
}
