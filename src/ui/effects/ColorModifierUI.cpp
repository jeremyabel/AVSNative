#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/ColorModifier.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

static void DrawColorModifierUI(Effect* base)
{
    auto* fx = static_cast<ColorModifier*>(base);

    ImGui::TextUnformatted("Pixel (GLSL)");
    if (ConfigUi::CodeEditor("cmod.pixelCode", fx->PixelCode, ConfigUi::Lang::Glsl))
        fx->RecompileMain();
    ScriptError(fx, ColorModifier::kPixelCode);

    ImGui::TextUnformatted("Init");
    if (ConfigUi::CodeEditor("cmod.initCode", fx->InitCode, ConfigUi::Lang::Lua))
        fx->RecompileMain();
    ScriptError(fx, ColorModifier::kInitCode);

    ImGui::TextUnformatted("Frame");
    if (ConfigUi::CodeEditor("cmod.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
        fx->RecompileFrameCode();
    ScriptError(fx, ColorModifier::kFrameCode);

    ImGui::TextUnformatted("Beat");
    if (ConfigUi::CodeEditor("cmod.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
        fx->RecompileBeatCode();
    ScriptError(fx, ColorModifier::kBeatCode);
}

void RegisterColorModifierUI(ConfigUiRegistry& reg)
{
    reg.Register("Color Modifier", &DrawColorModifierUI);
}
