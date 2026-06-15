#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/DynamicShift.h"

#include <imgui.h>

static void DrawDynamicShiftUI(Effect* base)
{
    auto* fx = static_cast<DynamicShift*>(base);

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dshift.initCode", fx->InitCode, ConfigUi::Lang::Lua))
            fx->RecompileInitCode();
        ConfigUi::ScriptError(fx, DynamicShift::NAME_InitCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dshift.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
            fx->RecompileBeatCode();
        ConfigUi::ScriptError(fx, DynamicShift::NAME_BeatCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dshift.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
            fx->RecompileFrameCode();
        ConfigUi::ScriptError(fx, DynamicShift::NAME_FrameCode);
        ImGui::TreePop();
    }

    ImGui::Checkbox("Blend", &fx->EnableBlend);
    ImGui::Checkbox("Subpixel", &fx->Bilinear);

    ImGui::BeginDisabled(!fx->Bilinear);
    ImGui::Checkbox("Bilinear (precise)", &fx->Compat);
    ImGui::EndDisabled();
}

void RegisterDynamicShiftUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Shift", &DrawDynamicShiftUI);
}
