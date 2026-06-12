#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Texer2.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

// ── Bespoke UI ────────────────────────────────────────────────────────────────

static void DrawTexer2UI(Effect* effect)
{
    auto* tex = static_cast<Texer2*>(effect);

    const int imgW   = tex->GetImageW();
    const int imgH   = tex->GetImageH();
    const int frames = tex->GetFrameCount();
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
    bool arrayMode = (tex->Mode == 1);
    if (ImGui::Checkbox("Keyed image array", &arrayMode))
    {
        tex->Mode = arrayMode ? 1 : 0;
        tex->LoadSelected();
    }

    if (tex->Mode == 0)
    {
        if (ImGui::Button("Load Image..."))
            ConfigUi::PickImageInto(effect, Texer2::kImageData);
    }
    else
    {
        if (ConfigUi::KeyedImageListEditor(effect, tex->Keyed))
            tex->LoadSelected();
    }

    ImGui::Spacing();

    ImGui::Checkbox("Resizing", &tex->Resize);
    ImGui::Checkbox("Wrap Around", &tex->Wrap);
    ImGui::Checkbox("Color Filtering", &tex->Colorize);

    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("texer2.initCode", tex->InitCode, ConfigUi::Lang::Lua))
            tex->RecompileInitCode();
        ScriptError(tex, Texer2::kInitCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("texer2.frameCode", tex->FrameCode, ConfigUi::Lang::Lua))
            tex->RecompileFrameCode();
        ScriptError(tex, Texer2::kFrameCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("texer2.beatCode", tex->BeatCode, ConfigUi::Lang::Lua))
            tex->RecompileBeatCode();
        ScriptError(tex, Texer2::kBeatCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Point", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("texer2.pointCode", tex->PointCode, ConfigUi::Lang::Lua))
            tex->RecompilePointCode();
        ScriptError(tex, Texer2::kPointCode);
        ImGui::TreePop();
    }
}

void RegisterTexer2UI(ConfigUiRegistry& reg)
{
    reg.Register("Texer II", DrawTexer2UI);
}
