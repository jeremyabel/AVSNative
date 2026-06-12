#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/DynamicShift.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

static void DrawDynamicShiftUI(Effect* base)
{
    auto* fx = static_cast<DynamicShift*>(base);

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dshift.initCode", fx->InitCode, ConfigUi::Lang::Lua))
            fx->RecompileInitCode();
        ScriptError(fx, DynamicShift::kInitCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dshift.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
            fx->RecompileFrameCode();
        ScriptError(fx, DynamicShift::kFrameCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dshift.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
            fx->RecompileBeatCode();
        ScriptError(fx, DynamicShift::kBeatCode);
        ImGui::TreePop();
    }

    ImGui::Checkbox("Blend", &fx->Blend);
    ImGui::Checkbox("Subpixel", &fx->Subpixel);

    ImGui::BeginDisabled(!fx->Subpixel);
    ImGui::Checkbox("Bilinear (precise)", &fx->Compat);
    ImGui::EndDisabled();
}

void RegisterDynamicShiftUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Shift", &DrawDynamicShiftUI);
}
