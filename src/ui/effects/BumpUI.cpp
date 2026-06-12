#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Bump.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

static void DrawBumpUI(Effect* base)
{
    auto* fx = static_cast<Bump*>(base);

    ImGui::TextUnformatted("Depth");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##depth", &fx->Depth, 1, 100);

    ImGui::Checkbox("On Beat", &fx->OnBeat);

    ImGui::TextUnformatted("On Beat Duration");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##onBeatDuration", &fx->OnBeatDuration, 0, 100);

    ImGui::TextUnformatted("On Beat Depth");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##onBeatDepth", &fx->OnBeatDepth, 1, 100);

    static const char* kBlends[] = { "Replace", "Additive", "50/50" };
    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &fx->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::Checkbox("Show Light Pos", &fx->ShowLightPos);
    ImGui::Checkbox("Invert Depth", &fx->InvertDepth);

    ImGui::TextUnformatted("Init");
    if (ConfigUi::CodeEditor("bump.initCode", fx->InitCode, ConfigUi::Lang::Lua))
        fx->RecompileCode();
    ScriptError(fx, Bump::kInitCode);

    ImGui::TextUnformatted("Frame");
    if (ConfigUi::CodeEditor("bump.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
        fx->RecompileCode();
    ScriptError(fx, Bump::kFrameCode);

    ImGui::TextUnformatted("Beat");
    if (ConfigUi::CodeEditor("bump.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
        fx->RecompileCode();
    ScriptError(fx, Bump::kBeatCode);
}

void RegisterBumpUI(ConfigUiRegistry& reg)
{
    reg.Register("Bump", &DrawBumpUI);
}
