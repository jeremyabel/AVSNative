#include "ui/ConfigUiRegistry.h"
#include "ui/effects/CustomBpmUI.h"

#include "effects/CustomBpm.h"

#include <imgui.h>

#include <algorithm>
#include <unordered_map>

// The beat simulation lives in CustomBpm::Render(); the effect exposes monotonic
// in/out beat counts. This UI edits the parameters (enforcing mode exclusivity) and
// animates the before/after meters from those counts — the bounce position is
// UI-only state, kept per effect instance here.

static std::unordered_map<CustomBpm*, CustomBpmUIState> s_state;

// Advance a bouncing 0..7 meter by `steps` beats (capped: when the panel was hidden
// the exact landing spot doesn't matter, and this bounds the loop).
static void AdvanceMeter(int& seg, int& dir, int steps)
{
    steps = std::clamp(steps, 0, 64);
    for (int i = 0; i < steps; ++i)
    {
        seg += dir;
        if      (seg >= 7) { seg = 7; dir = -1; }
        else if (seg <= 0) { seg = 0; dir =  1; }
    }
}

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
    CustomBpmUIState& ui = s_state[bpm];

    // ── Mode selection (mutually exclusive; turning one on clears the others) ──
    ImGui::SeparatorText("Mode");
    if (ImGui::Checkbox("Arbitrary BPM", &bpm->Arbitrary) && bpm->Arbitrary)
        { bpm->Skip = false; bpm->Invert = false; }
    if (ImGui::Checkbox("Skip Beats", &bpm->Skip) && bpm->Skip)
        { bpm->Arbitrary = false; bpm->Invert = false; }
    if (ImGui::Checkbox("Invert Beat", &bpm->Invert) && bpm->Invert)
        { bpm->Arbitrary = false; bpm->Skip = false; }

    ImGui::Spacing();

    ImGui::BeginDisabled(!bpm->Arbitrary);
    ImGui::TextUnformatted("Arbitrary BPM");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##arbval", &bpm->ArbVal, 6, 300);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!bpm->Skip);
    ImGui::TextUnformatted("Skip");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##skipval", &bpm->SkipVal, 1, 16);
    ImGui::EndDisabled();

    ImGui::TextUnformatted("Skip First N");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##skipfirst", &bpm->SkipFirst, 0, 64);

    // ── Before / after beat meters (advanced by new beats since last frame) ──
    AdvanceMeter(ui.InSeg,  ui.InDir,  bpm->GetInBeatCount()  - ui.LastInCount);
    AdvanceMeter(ui.OutSeg, ui.OutDir, bpm->GetOutBeatCount() - ui.LastOutCount);
    ui.LastInCount  = bpm->GetInBeatCount();
    ui.LastOutCount = bpm->GetOutBeatCount();

    ImGui::SeparatorText("Beat Meter");
    DrawBeatMeter("In",  ui.InSeg);
    DrawBeatMeter("Out", ui.OutSeg);
}

void RegisterCustomBpmUI(ConfigUiRegistry& reg)
{
    reg.Register("Custom BPM", DrawCustomBpmUI);
}
