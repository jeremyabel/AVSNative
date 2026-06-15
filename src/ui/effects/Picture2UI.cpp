#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Picture2.h"

#include <imgui.h>

// ── Blend mode combo helper ───────────────────────────────────────────────────

static const char* kBlendLabels[] = {
    "Replace", "Additive", "Maximum", "50/50",
    "Subtractive 1", "Subtractive 2", "Multiply",
    "Adjustable", "XOR", "Minimum", "Ignore",
};
static constexpr int kNumBlendModes = 11;

static bool BlendCombo(const char* label, int& mode)
{
    const char* preview = (mode >= 0 && mode < kNumBlendModes) ? kBlendLabels[mode] : "?";
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        for (int i = 0; i < kNumBlendModes; ++i) {
            bool sel = (i == mode);
            if (ImGui::Selectable(kBlendLabels[i], sel)) { mode = i; changed = true; }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

// ── Bespoke UI ────────────────────────────────────────────────────────────────

static void DrawPicture2UI(Effect* effect)
{
    auto* pic = static_cast<Picture2*>(effect);

    // Image status
    const int imgW = pic->GetImageW();
    const int imgH = pic->GetImageH();
    if (imgW > 0 && imgH > 0)
        ImGui::Text("Image: %d x %d", imgW, imgH);
    else
        ImGui::Text("No image loaded");

    if (ImGui::Button("Load Image..."))
        ConfigUi::PickImageInto(effect, Picture2::kImageData);

    ImGui::Spacing();
    ImGui::SeparatorText("Normal");

    ImGui::Text("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    BlendCombo("##blend", pic->BlendMode);

    if (pic->BlendMode == 7) {
        ImGui::Text("Blend Amount");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##adj", &pic->AdjustBlend, 0, 255);
    }

    ImGui::Checkbox("Bilinear", &pic->Bilinear);

    ImGui::Spacing();
    ImGui::SeparatorText("On Beat");

    ImGui::Text("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    BlendCombo("##obblend", pic->OnBeatBlendMode);

    if (pic->OnBeatBlendMode == 7) {
        ImGui::Text("Blend Amount");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##obadj", &pic->OnBeatAdjustBlend, 0, 255);
    }

    ImGui::Checkbox("On-Beat Bilinear", &pic->OnBeatBilinear);
}

void RegisterPicture2UI(ConfigUiRegistry& reg)
{
    reg.Register("Picture II", DrawPicture2UI);
}
