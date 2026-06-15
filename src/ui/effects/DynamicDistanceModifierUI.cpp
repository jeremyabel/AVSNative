#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/DynamicDistanceModifier.h"

#include <imgui.h>

static void DrawDynamicDistanceModifierUI(Effect* base)
{
    auto* fx = static_cast<DynamicDistanceModifier*>(base);

    ImGui::Checkbox("Blend", &fx->Blend);

    ImGui::Checkbox("Bilinear Filtering", &fx->Bilinear);
    if (fx->Bilinear)
    {
        ImGui::SameLine();
        ImGui::Checkbox("Precise", &fx->Compat);
    }

    ImGui::Separator();

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("ddm.initCode", fx->InitCode, ConfigUi::Lang::Lua))
            fx->RecompileMain();
        ConfigUi::ScriptError(fx, DynamicDistanceModifier::NAME_InitCode);
        ImGui::TreePop();
    }
    
    ImGui::Spacing();
    
    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("ddm.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
        fx->RecompileBeatCode();
        ConfigUi::ScriptError(fx, DynamicDistanceModifier::NAME_BeatCode);
        ImGui::TreePop();
    }
    
    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("ddm.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
            fx->RecompileFrameCode();
        ConfigUi::ScriptError(fx, DynamicDistanceModifier::NAME_FrameCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Pixel (GLSL)", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("ddm.pixelCode", fx->PixelCode, ConfigUi::Lang::Glsl))
            fx->RecompileMain();
        ConfigUi::ScriptError(fx, DynamicDistanceModifier::NAME_PixelCode);
        ImGui::TreePop();
    }
}

void RegisterDynamicDistanceModifierUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Distance Modifier", &DrawDynamicDistanceModifierUI);
}
