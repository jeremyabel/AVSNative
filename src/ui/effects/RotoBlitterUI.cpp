#include "ui/ConfigUiRegistry.h"

#include "effects/RotoBlitter.h"

#include <imgui.h>

static void DrawRotoBlitterUI(Effect* base)
{
    auto* fx = static_cast<RotoBlitter*>(base);

    ImGui::Text("Zoom");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##zoom_scale", &fx->ZoomScale, 0, 256))
        fx->ResetZoomAnim();

    ImGui::Text("Zoom (On Beat)");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##zoom_scale2", &fx->ZoomScale2, 0, 256);

    ImGui::Text("Rotation");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##rot_dir", &fx->RotDir, 0, 64);

    ImGui::Text("Reversal Smoothing");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##beatch_speed", &fx->BeatchSpeed, 0, 8);

    ImGui::Checkbox("Subpixel", &fx->Subpixel);

    ImGui::BeginDisabled(!fx->Subpixel);
    ImGui::Checkbox("Bilinear (precise)", &fx->Compat);
    ImGui::EndDisabled();

    ImGui::Checkbox("Blend", &fx->Blend);
    ImGui::Checkbox("Reverse on Beat", &fx->Beatch);
    ImGui::Checkbox("Zoom Snap on Beat", &fx->BeatchScale);
}

void RegisterRotoBlitterUI(ConfigUiRegistry& reg)
{
    reg.Register("Roto Blitter", &DrawRotoBlitterUI);
}
