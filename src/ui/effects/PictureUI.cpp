#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Picture.h"

#include <imgui.h>

// ── Bespoke UI ────────────────────────────────────────────────────────────────

static void DrawPictureUI(Effect* effect)
{
    auto* pic = static_cast<Picture*>(effect);

    const int imgW = pic->GetImageW();
    const int imgH = pic->GetImageH();
    if (imgW > 0 && imgH > 0)
        ImGui::Text("Image: %d x %d", imgW, imgH);
    else
        ImGui::TextUnformatted("No image loaded");

    if (ImGui::Button("Load Image..."))
        ConfigUi::PickImageInto(effect, Picture::NAME_ImageData);

    ImGui::Spacing();

    static const char* kBlends[] = { "Replace", "Additive", "50/50" };
    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &pic->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::Checkbox("On-Beat Additive", &pic->OnBeatAdditive);

    ImGui::TextUnformatted("On-Beat Duration");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##onBeatDuration", &pic->OnBeatDuration, 0, 32);

    static const char* kFits[] = { "Stretch", "Fit Width", "Fit Height" };
    ImGui::TextUnformatted("Image Fit");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##fit", &pic->Fit, kFits, IM_ARRAYSIZE(kFits));
}

void RegisterPictureUI(ConfigUiRegistry& reg)
{
    reg.Register("Picture", DrawPictureUI);
}
