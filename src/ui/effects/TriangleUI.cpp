#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Triangle.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

static void DrawTriangleUI(Effect* base)
{
    auto* fx = static_cast<Triangle*>(base);

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("triangle.initCode", fx->InitCode, ConfigUi::Lang::Lua))
            fx->RecompileAll();
        ScriptError(fx, Triangle::kInitCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("triangle.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
            fx->RecompileAll();
        ScriptError(fx, Triangle::kFrameCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("triangle.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
            fx->RecompileAll();
        ScriptError(fx, Triangle::kBeatCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Triangle", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("triangle.triangleCode", fx->TriangleCode, ConfigUi::Lang::Lua))
            fx->RecompileAll();
        ScriptError(fx, Triangle::kTriangleCode);
        ImGui::TreePop();
    }

    ImGui::Checkbox("Antialiasing", &fx->AntialiasingEnabled);
}

void RegisterTriangleUI(ConfigUiRegistry& reg)
{
    reg.Register("Triangle", &DrawTriangleUI);
}
