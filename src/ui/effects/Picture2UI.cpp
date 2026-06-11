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
    Picture2Config& cfg = pic->ConfigRef();

    // Image status
    const int imgW = pic->GetImageW();
    const int imgH = pic->GetImageH();
    if (imgW > 0 && imgH > 0)
        ImGui::Text("Image: %d x %d", imgW, imgH);
    else
        ImGui::TextUnformatted("No image loaded");

    if (ImGui::Button("Load Image..."))
        ConfigUi::PickImageInto(effect, "imageData");

    ImGui::Spacing();
    ImGui::SeparatorText("Normal");

    bool dirty = false;

    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    dirty |= BlendCombo("##blend", cfg.BlendMode);

    if (cfg.BlendMode == 7) {
        ImGui::TextUnformatted("Blend Amount");
        ImGui::SetNextItemWidth(-1.0f);
        dirty |= ImGui::SliderInt("##adj", &cfg.AdjustBlend, 0, 255);
    }

    dirty |= ImGui::Checkbox("Bilinear", &cfg.Bilinear);

    ImGui::Spacing();
    ImGui::SeparatorText("On Beat");

    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    dirty |= BlendCombo("##obblend", cfg.OnBeatBlendMode);

    if (cfg.OnBeatBlendMode == 7) {
        ImGui::TextUnformatted("Blend Amount");
        ImGui::SetNextItemWidth(-1.0f);
        dirty |= ImGui::SliderInt("##obadj", &cfg.OnBeatAdjustBlend, 0, 255);
    }

    dirty |= ImGui::Checkbox("On-Beat Bilinear", &cfg.OnBeatBilinear);

    if (dirty)
        pic->NotifyConfigChanged({ "blendMode", "adjustBlend", "bilinear",
                                   "onBeatBlendMode", "onBeatAdjustBlend", "onBeatBilinear" });
}

void RegisterPicture2UI(ConfigUiRegistry& reg)
{
    reg.Register("Picture II", DrawPicture2UI);
}
