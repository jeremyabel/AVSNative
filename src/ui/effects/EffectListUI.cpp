#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/EffectList.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

static void DrawEffectListUI(Effect* base)
{
    auto* fx = static_cast<EffectList*>(base);

    static const char* kBlends[] = { "Ignore", "Replace", "50/50", "Maximum", "Additive",
                                     "Subtractive 1", "Subtractive 2", "Every Other Line",
                                     "Every Other Pixel", "XOR", "Adjustable", "Multiply",
                                     "Buffer", "Minimum" };
    static const char* kSlots[] = { "0", "1", "2", "3", "4", "5", "6", "7" };

    ImGui::Checkbox("Enable on Beat", &fx->OnBeat);

    ImGui::TextUnformatted("For N Frames");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputInt("##onBeatFrames", &fx->OnBeatFrames);

    ImGui::Checkbox("Clear Frame", &fx->ClearFrame);

    ImGui::TextUnformatted("Input Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##inBlend", &fx->InBlend, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::TextUnformatted("Input Buffer");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##inBlendBuf", &fx->InBlendBuf, kSlots, IM_ARRAYSIZE(kSlots));

    ImGui::Checkbox("Invert Input Mask", &fx->InBlendBufInvert);

    ImGui::TextUnformatted("Output Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##outBlend", &fx->OutBlend, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::TextUnformatted("Output Buffer");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##outBlendBuf", &fx->OutBlendBuf, kSlots, IM_ARRAYSIZE(kSlots));

    ImGui::Checkbox("Invert Output Mask", &fx->OutBlendBufInvert);

    ImGui::TextUnformatted("Blend Amount");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##blendAmt", &fx->BlendAmt, 0.0f, 1.0f);

    ImGui::Checkbox("Use evaluation override", &fx->UseEval);

    ImGui::TextUnformatted("Init");
    if (ConfigUi::CodeEditor("effectlist.initCode", fx->InitCode, ConfigUi::Lang::Lua))
        fx->RecompileInitCode();
    ScriptError(fx, EffectList::kInitCode);

    ImGui::TextUnformatted("Frame");
    if (ConfigUi::CodeEditor("effectlist.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
        fx->RecompileFrameCode();
    ScriptError(fx, EffectList::kFrameCode);
}

void RegisterEffectListUI(ConfigUiRegistry& reg)
{
    reg.Register("Effect List", &DrawEffectListUI);
}
