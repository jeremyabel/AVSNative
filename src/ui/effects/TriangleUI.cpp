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

    ImGui::TextUnformatted("Init");
    if (ConfigUi::CodeEditor("triangle.initCode", fx->InitCode, ConfigUi::Lang::Lua))
        fx->RecompileAll();
    ScriptError(fx, Triangle::kInitCode);

    ImGui::TextUnformatted("Frame");
    if (ConfigUi::CodeEditor("triangle.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
        fx->RecompileAll();
    ScriptError(fx, Triangle::kFrameCode);

    ImGui::TextUnformatted("Beat");
    if (ConfigUi::CodeEditor("triangle.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
        fx->RecompileAll();
    ScriptError(fx, Triangle::kBeatCode);

    ImGui::TextUnformatted("Triangle");
    if (ConfigUi::CodeEditor("triangle.triangleCode", fx->TriangleCode, ConfigUi::Lang::Lua))
        fx->RecompileAll();
    ScriptError(fx, Triangle::kTriangleCode);

    ImGui::Checkbox("Antialiasing", &fx->AntialiasingEnabled);
}

void RegisterTriangleUI(ConfigUiRegistry& reg)
{
    reg.Register("Triangle", &DrawTriangleUI);
}
