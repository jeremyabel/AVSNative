#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Texer.h"

#include <imgui.h>

// ── Bespoke UI ────────────────────────────────────────────────────────────────

static void DrawTexerUI(Effect* effect)
{
    auto* tex = static_cast<Texer*>(effect);

    const int imgW = tex->GetImageW();
    const int imgH = tex->GetImageH();
    const int frames = tex->GetFrameCount();
    if (imgW > 0 && imgH > 0)
    {
        if (frames > 1)
            ImGui::Text("Image: %d x %d  (animated, %d frames)", imgW, imgH, frames);
        else
            ImGui::Text("Image: %d x %d", imgW, imgH);
    }
    else
    {
        ImGui::Text("No image loaded");
    }

    if (ImGui::Button("Load Image..."))
    {
        ConfigUi::PickImageInto(effect, Texer::kImageData);
    }

    ImGui::Spacing();

    ImGui::Checkbox("Add to Input", &tex->AddToInput);
    ImGui::Checkbox("Colorize", &tex->Colorize);

    ImGui::Text("Particles");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##numParticles", &tex->NumParticles, 1, 1024);
}

void RegisterTexerUI(ConfigUiRegistry& reg)
{
    reg.Register("Texer", DrawTexerUI);
}
