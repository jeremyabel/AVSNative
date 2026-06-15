#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Text.h"

#include <imgui.h>

#include <cstdio>

static void DrawTextUI(Effect* effect)
{
    auto* t = static_cast<Text*>(effect);

    char textBuf[1024];
    std::snprintf(textBuf, sizeof(textBuf), "%s", t->TextString.c_str());
    ImGui::Text("Text (;-separated words)");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##text", textBuf, sizeof(textBuf)))
        t->TextString = textBuf;

    char fontBuf[128];
    std::snprintf(fontBuf, sizeof(fontBuf), "%s", t->FontFamily.c_str());
    ImGui::Text("Font Family");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##fontFamily", fontBuf, sizeof(fontBuf)))
        t->FontFamily = fontBuf;

    ImGui::Spacing();

    ImGui::Text("Font Size");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##fontSize", &t->FontSize, 8, 200);

    ImGui::Checkbox("Bold", &t->Bold);
    ImGui::SameLine();
    ImGui::Checkbox("Italic", &t->Italic);

    ImGui::Text("Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##color", t->Color);

    ImGui::Text("Outline/Shadow Color");
    ImGui::SetNextItemWidth(-1.0f);
    ConfigUi::ColorEdit("##outlineColor", t->OutlineColor);

    ImGui::Checkbox("Outline", &t->Outline);
    ImGui::SameLine();
    ImGui::Checkbox("Legacy Outline", &t->LegacyOutline);
    ImGui::SameLine();
    ImGui::Checkbox("Shadow", &t->Shadow);

    ImGui::Text("Outline/Shadow Size");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##outlineSize", &t->OutlineSize, 1, 16);

    static const char* kBlends[] = { "Replace", "Additive", "50/50" };
    ImGui::Text("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &t->Blend, kBlends, IM_ARRAYSIZE(kBlends));

    static const char* kHAligns[] = { "Left", "Center", "Right" };
    ImGui::Text("Horizontal Align");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##halign", &t->HAlign, kHAligns, IM_ARRAYSIZE(kHAligns));

    static const char* kVAligns[] = { "Top", "Middle", "Bottom" };
    ImGui::Text("Vertical Align");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##valign", &t->VAlign, kVAligns, IM_ARRAYSIZE(kVAligns));

    ImGui::Text("X Shift (%)");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##xshift", &t->XShift, 0, 100);

    ImGui::Text("Y Shift (%)");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##yshift", &t->YShift, 0, 100);

    ImGui::Checkbox("Random Position", &t->RandomPos);
    ImGui::Checkbox("On Beat", &t->OnBeat);

    ImGui::Text("Frames Per Word");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##normSpeed", &t->NormSpeed, 1, 256);

    ImGui::Text("Visible Frames");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##onbeatSpeed", &t->OnBeatSpeed, 1, 256);

    ImGui::Checkbox("Insert Blank", &t->InsertBlank);
    ImGui::Checkbox("Random Word", &t->RandomWord);
}

void RegisterTextUI(ConfigUiRegistry& reg)
{
    reg.Register("Text", DrawTextUI);
}
