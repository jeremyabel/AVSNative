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
        ConfigUi::PickImageInto(effect, "imageData");

    ImGui::Spacing();

    // Reflected params (BlendMode, OnBeatAdditive, OnBeatDuration, Fit)
    DrawDefault(effect);
}

void RegisterPictureUI(ConfigUiRegistry& reg)
{
    reg.Register("Picture", DrawPictureUI);
}
