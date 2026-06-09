#include "ui/ConfigUiRegistry.h"

#include "effects/Text.h"

#include <imgui/imgui.h>

#include <cstring>

// Bespoke UI: the text string and font family need free-text inputs (no string param
// type in the auto layout); everything else is handled by DrawDefault.
static void DrawTextUI(Effect* effect)
{
    auto* t = static_cast<Text*>(effect);
    TextConfig& cfg = t->ConfigRef();

    char textBuf[1024];
    std::snprintf(textBuf, sizeof(textBuf), "%s", cfg.Text.c_str());
    ImGui::TextUnformatted("Text (;-separated words)");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##text", textBuf, sizeof(textBuf)))
    {
        cfg.Text = textBuf;
        t->NotifyConfigChanged({ "text" });
    }

    char fontBuf[128];
    std::snprintf(fontBuf, sizeof(fontBuf), "%s", cfg.FontFamily.c_str());
    ImGui::TextUnformatted("Font Family");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##fontFamily", fontBuf, sizeof(fontBuf)))
    {
        cfg.FontFamily = fontBuf;
        t->NotifyConfigChanged({ "fontFamily" });
    }

    ImGui::Spacing();
    DrawDefault(effect);
}

void RegisterTextUI(ConfigUiRegistry& reg)
{
    reg.Register("Text", DrawTextUI);
}
