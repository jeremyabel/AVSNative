#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/ImageGrid.h"

#include <imgui.h>

// ── Bespoke UI ────────────────────────────────────────────────────────────────

static void DrawImageGridUI(Effect* effect)
{
    auto* grid = static_cast<ImageGrid*>(effect);
    auto& cfg  = grid->ConfigRef();

    const int imgW   = grid->GetImageW();
    const int imgH   = grid->GetImageH();
    const int frames = grid->GetFrameCount();
    if (imgW > 0 && imgH > 0)
    {
        if (frames > 1)
            ImGui::Text("Image: %d x %d  (animated, %d frames)", imgW, imgH, frames);
        else
            ImGui::Text("Image: %d x %d", imgW, imgH);
    }
    else
        ImGui::TextUnformatted("No image loaded");

    // Two image slots with an A/B toggle, for testing instant GIF switching.
    ImGui::Text("Active: Image %d", cfg.ActiveImage + 1);

    if (ImGui::Button("Load Image 1..."))
        ConfigUi::PickImageInto(effect, "imageData");
    ImGui::SameLine();
    ImGui::TextDisabled(cfg.ImageData.empty() ? "(empty)" : "(loaded)");

    if (ImGui::Button("Load Image 2..."))
        ConfigUi::PickImageInto(effect, "imageData2");
    ImGui::SameLine();
    ImGui::TextDisabled(cfg.ImageData2.empty() ? "(empty)" : "(loaded)");

    if (ImGui::Button("Toggle Image"))
        effect->SetConfig({ { "activeImage", cfg.ActiveImage ^ 1 } });

    ImGui::Spacing();
    DrawDefault(effect);
}

void RegisterImageGridUI(ConfigUiRegistry& reg)
{
    reg.Register("Image Grid", DrawImageGridUI);
}
