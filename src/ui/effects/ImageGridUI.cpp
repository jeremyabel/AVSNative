#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/ImageGrid.h"

#include <imgui.h>

// ── Bespoke UI ────────────────────────────────────────────────────────────────

static void DrawImageGridUI(Effect* effect)
{
    auto* grid = static_cast<ImageGrid*>(effect);

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

    // Image source: a single image, or a keyed array switchable by keyboard.
    bool arrayMode = (grid->Mode == 1);
    if (ImGui::Checkbox("Keyed image array", &arrayMode))
    {
        grid->Mode = arrayMode ? 1 : 0;
        grid->LoadSelected();
    }

    if (grid->Mode == 0)
    {
        if (ImGui::Button("Load Image..."))
            ConfigUi::PickImageInto(effect, ImageGrid::kImageData);
        ImGui::SameLine();
        ImGui::TextDisabled(grid->ImageData.empty() ? "(empty)" : "(loaded)");
    }
    else
    {
        if (ConfigUi::KeyedImageListEditor(effect, grid->Keyed))
            grid->LoadSelected();
    }

    ImGui::Spacing();

    static const char* kBlends[] = { "Replace", "Additive", "50/50", "Alpha" };
    ImGui::TextUnformatted("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &grid->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("imagegrid.initCode", grid->InitCode, ConfigUi::Lang::Lua))
            grid->RecompileInitCode();
        ConfigUi::ScriptError(grid, ImageGrid::kInitCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("imagegrid.frameCode", grid->FrameCode, ConfigUi::Lang::Lua))
            grid->RecompileFrameCode();
        ConfigUi::ScriptError(grid, ImageGrid::kFrameCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("imagegrid.beatCode", grid->BeatCode, ConfigUi::Lang::Lua))
            grid->RecompileBeatCode();
        ConfigUi::ScriptError(grid, ImageGrid::kBeatCode);
        ImGui::TreePop();
    }
}

void RegisterImageGridUI(ConfigUiRegistry& reg)
{
    reg.Register("Image Grid", DrawImageGridUI);
}
