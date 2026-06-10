#include "ui/ConfigUiRegistry.h"

#include "effects/CustomBpm.h"

#include <imgui.h>

// The beat simulation lives in CustomBpm::Render(). This UI only edits the
// parameters and displays the before/after beat meters the effect computes.

static void DrawBeatMeter(const char* label, int seg)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine(48.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p    = ImGui::GetCursorScreenPos();
    const float  h    = ImGui::GetFrameHeight();
    const float  segW = 14.0f, gap = 3.0f;

    for (int i = 0; i < 8; ++i)
    {
        const ImVec2 a(p.x + i * (segW + gap), p.y);
        const ImVec2 b(a.x + segW, p.y + h);
        const ImU32 col = (i == seg) ? IM_COL32(90, 220, 130, 255)
                                     : IM_COL32(48, 48, 52, 255);
        dl->AddRectFilled(a, b, col, 2.0f);
    }
    ImGui::Dummy(ImVec2(8 * (segW + gap), h));
}

static void DrawCustomBpmUI(Effect* effect)
{
    auto* bpm = static_cast<CustomBpm*>(effect);
    CustomBpmConfig& cfg = bpm->ConfigRef();

    // ── Mode selection (mutually exclusive; OnConfigChanged enforces it) ──
    ImGui::SeparatorText("Mode");
    bool modeChanged = false;
    modeChanged |= ImGui::Checkbox("Arbitrary BPM", &cfg.Arbitrary);
    modeChanged |= ImGui::Checkbox("Skip Beats",    &cfg.Skip);
    modeChanged |= ImGui::Checkbox("Invert Beat",   &cfg.Invert);
    if (modeChanged)
        bpm->NotifyConfigChanged({ "arbitrary", "skip", "invert" });

    ImGui::Spacing();

    ImGui::BeginDisabled(!cfg.Arbitrary);
    ImGui::TextUnformatted("Arbitrary BPM");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##arbval", &cfg.ArbVal, 6, 300);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!cfg.Skip);
    ImGui::TextUnformatted("Skip");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##skipval", &cfg.SkipVal, 1, 16);
    ImGui::EndDisabled();

    ImGui::TextUnformatted("Skip First N");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##skipfirst", &cfg.SkipFirst, 0, 64);

    // ── Before / after beat meters (computed by the effect) ──
    ImGui::SeparatorText("Beat Meter");
    DrawBeatMeter("In",  bpm->InMeterSeg());
    DrawBeatMeter("Out", bpm->OutMeterSeg());
}

void RegisterCustomBpmUI(ConfigUiRegistry& reg)
{
    reg.Register("Custom BPM", DrawCustomBpmUI);
}
