#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/EffectList.h"

#include <imgui.h>

static void DrawEffectListUI(Effect* base)
{
    auto* fx = static_cast<EffectList*>(base);

    static const char* kBlends[] = { "Ignore", "Replace", "50/50", "Maximum", "Additive",
                                     "Subtractive 1", "Subtractive 2", "Every Other Line",
                                     "Every Other Pixel", "XOR", "Adjustable", "Multiply",
                                     "Buffer", "Minimum" };
    static const char* kSlots[] = { "1", "2", "3", "4", "5", "6", "7", "8" };

    ImGui::Checkbox("Enable on Beat", &fx->OnBeat);
    if (fx->OnBeat)
    {
        ImGui::SameLine();
        ImGui::TextUnformatted("For N Frames");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("##onBeatFrames", &fx->OnBeatFrames);
    }

    ImGui::Checkbox("Clear Frame", &fx->ClearFrame);

    ImGui::TextUnformatted("Input Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##inBlend", &fx->InBlend, kBlends, IM_ARRAYSIZE(kBlends));

    if (fx->InBlend == 12)
    {
        ImGui::TextUnformatted("Input Buffer");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("##inBlendBuf", &fx->InBlendBuf, kSlots, IM_ARRAYSIZE(kSlots));
    
        ImGui::Checkbox("Invert Input Mask", &fx->InBlendBufInvert);
    }

    ImGui::TextUnformatted("Output Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##outBlend", &fx->OutBlend, kBlends, IM_ARRAYSIZE(kBlends));

    if (fx->OutBlend == 12)
    {
        ImGui::TextUnformatted("Output Buffer");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("##outBlendBuf", &fx->OutBlendBuf, kSlots, IM_ARRAYSIZE(kSlots));
    
        ImGui::Checkbox("Invert Output Mask", &fx->OutBlendBufInvert);
    }

    ImGui::TextUnformatted("Blend Amount");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##blendAmt", &fx->BlendAmt, 0.0f, 1.0f);

    ImGui::Checkbox("Use evaluation override", &fx->UseEval);
    ImGui::Separator();

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("effectlist.initCode", fx->InitCode, ConfigUi::Lang::Lua))
            fx->RecompileInitCode();
        ConfigUi::ScriptError(fx, EffectList::kInitCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("effectlist.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
            fx->RecompileFrameCode();
        ConfigUi::ScriptError(fx, EffectList::kFrameCode);
        ImGui::TreePop();
    }
}

void RegisterEffectListUI(ConfigUiRegistry& reg)
{
    reg.Register("Effect List", &DrawEffectListUI);
}
