#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/ColorModifier.h"

#include <imgui.h>

static void DrawColorModifierUI(Effect* base)
{
    auto* fx = static_cast<ColorModifier*>(base);

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("cmod.initCode", fx->InitCode, ConfigUi::Lang::Lua))
            fx->RecompileMain();
        ConfigUi::ScriptError(fx, ColorModifier::NAME_InitCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("cmod.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
            fx->RecompileFrameCode();
        ConfigUi::ScriptError(fx, ColorModifier::NAME_FrameCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("cmod.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
            fx->RecompileBeatCode();
        ConfigUi::ScriptError(fx, ColorModifier::NAME_BeatCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Pixel (GLSL)", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("cmod.pixelCode", fx->PixelCode, ConfigUi::Lang::Glsl))
            fx->RecompileMain();
        ConfigUi::ScriptError(fx, ColorModifier::NAME_PixelCode);
        ImGui::TreePop();
    }
}

void RegisterColorModifierUI(ConfigUiRegistry& reg)
{
    reg.Register("Color Modifier", &DrawColorModifierUI);
}
