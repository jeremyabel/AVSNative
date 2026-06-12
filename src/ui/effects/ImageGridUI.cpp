#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/ImageGrid.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

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

    // Two image slots with an A/B toggle, for testing instant GIF switching.
    ImGui::Text("Active: Image %d", grid->ActiveImage + 1);

    if (ImGui::Button("Load Image 1..."))
        ConfigUi::PickImageInto(effect, ImageGrid::kImageData);
    ImGui::SameLine();
    ImGui::TextDisabled(grid->ImageData.empty() ? "(empty)" : "(loaded)");

    if (ImGui::Button("Load Image 2..."))
        ConfigUi::PickImageInto(effect, ImageGrid::kImageData2);
    ImGui::SameLine();
    ImGui::TextDisabled(grid->ImageData2.empty() ? "(empty)" : "(loaded)");

    if (ImGui::Button("Toggle Image"))
        grid->SetActiveImage(grid->ActiveImage ^ 1);

    ImGui::Spacing();

    static const char* kBlends[] = { "Replace", "Additive", "50/50", "Alpha" };
    ImGui::TextUnformatted("Blend");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##blendMode", &grid->BlendMode, kBlends, IM_ARRAYSIZE(kBlends));

    ImGui::TextUnformatted("Init");
    if (ConfigUi::CodeEditor("imagegrid.initCode", grid->InitCode, ConfigUi::Lang::Lua))
        grid->RecompileInitCode();
    ScriptError(grid, ImageGrid::kInitCode);

    ImGui::TextUnformatted("Frame");
    if (ConfigUi::CodeEditor("imagegrid.frameCode", grid->FrameCode, ConfigUi::Lang::Lua))
        grid->RecompileFrameCode();
    ScriptError(grid, ImageGrid::kFrameCode);

    ImGui::TextUnformatted("Beat");
    if (ConfigUi::CodeEditor("imagegrid.beatCode", grid->BeatCode, ConfigUi::Lang::Lua))
        grid->RecompileBeatCode();
    ScriptError(grid, ImageGrid::kBeatCode);
}

void RegisterImageGridUI(ConfigUiRegistry& reg)
{
    reg.Register("Image Grid", DrawImageGridUI);
}
