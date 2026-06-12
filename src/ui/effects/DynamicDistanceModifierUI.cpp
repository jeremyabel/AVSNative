#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/DynamicDistanceModifier.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

static void DrawDynamicDistanceModifierUI(Effect* base)
{
    auto* fx = static_cast<DynamicDistanceModifier*>(base);

    ImGui::Checkbox("Blend", &fx->Blend);
    ImGui::Checkbox("Bilinear Filtering", &fx->Bilinear);

    ImGui::BeginDisabled(!fx->Bilinear);
    ImGui::Checkbox("Bilinear (precise)", &fx->Compat);
    ImGui::EndDisabled();

    ImGui::TextUnformatted("Pixel (GLSL)");
    if (ConfigUi::CodeEditor("ddm.pixelCode", fx->PixelCode, ConfigUi::Lang::Glsl))
        fx->RecompileMain();
    ScriptError(fx, DynamicDistanceModifier::kPixelCode);

    ImGui::TextUnformatted("Init");
    if (ConfigUi::CodeEditor("ddm.initCode", fx->InitCode, ConfigUi::Lang::Lua))
        fx->RecompileMain();
    ScriptError(fx, DynamicDistanceModifier::kInitCode);

    ImGui::TextUnformatted("Frame");
    if (ConfigUi::CodeEditor("ddm.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
        fx->RecompileFrameCode();
    ScriptError(fx, DynamicDistanceModifier::kFrameCode);

    ImGui::TextUnformatted("Beat");
    if (ConfigUi::CodeEditor("ddm.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
        fx->RecompileBeatCode();
    ScriptError(fx, DynamicDistanceModifier::kBeatCode);
}

void RegisterDynamicDistanceModifierUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Distance Modifier", &DrawDynamicDistanceModifierUI);
}
