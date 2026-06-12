#include "ui/ConfigUiRegistry.h"

#include "effects/Ramp.h"

#include <imgui.h>
#include <imgui_gradient/imgui_gradient.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

// ── Per-effect editor state (gradient widget persists across frames) ───────────

struct RampUiState
{
    uint64_t             LastVersion = (uint64_t)-1;   // force initial resync
    ImGG::GradientWidget Widget;
};

static std::unordered_map<Effect*, RampUiState> g_states;

// ── Conversions between our stops and the library's marks ──────────────────────

static ImGG::GradientWidget BuildWidget(const std::vector<RampStop>& stops)
{
    std::list<ImGG::Mark> marks;
    for (const RampStop& s : stops)
    {
        const float pos = std::clamp(s.Position, 0, 255) / 255.0f;
        marks.push_back(ImGG::Mark{
            ImGG::RelativePosition{ pos },
            ImGG::ColorRGBA{ s.Color[0] / 255.0f, s.Color[1] / 255.0f,
                             s.Color[2] / 255.0f, 1.0f } });
    }
    return ImGG::GradientWidget{ marks };
}

static void WidgetToStops(const ImGG::GradientWidget& w, std::vector<RampStop>& out)
{
    std::vector<RampStop> stops;
    for (const ImGG::Mark& m : w.gradient().get_marks())
    {
        RampStop s;
        s.Position = std::clamp((int)std::lround(m.position.get() * 255.0f), 0, 255);
        s.Color = {
            (uint8_t)std::clamp((int)std::lround(m.color.x * 255.0f), 0, 255),
            (uint8_t)std::clamp((int)std::lround(m.color.y * 255.0f), 0, 255),
            (uint8_t)std::clamp((int)std::lround(m.color.z * 255.0f), 0, 255),
        };
        stops.push_back(s);
    }
    if (!stops.empty())
        out = std::move(stops);
}

// ── Bespoke UI ─────────────────────────────────────────────────────────────────

static void DrawRampUI(Effect* effect)
{
    auto* r = static_cast<Ramp*>(effect);
    RampUiState& st = g_states[effect];

    // Resync the gradient widget if the config changed externally (e.g. preset load).
    if (st.LastVersion != r->ConfigVersion())
    {
        st.Widget      = BuildWidget(r->Stops);
        st.LastVersion = r->ConfigVersion();
    }

    // ── Shape ──
    const char* typeOpts[] = { "Linear", "Radial", "Diamond", "Square" };
    ImGui::TextUnformatted("Type");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##type", typeOpts[std::clamp(r->Type, 0, 3)]))
    {
        for (int i = 0; i < 4; ++i)
            if (ImGui::Selectable(typeOpts[i], r->Type == i)) r->Type = i;
        ImGui::EndCombo();
    }

    ImGui::TextUnformatted("Scale");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##scale", &r->Scale, 0.1f, 8.0f, "%.2f");

    ImGui::TextUnformatted("Rotation");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##rot", &r->Rotation, 0.0f, 360.0f, "%.0f deg");

    // Aspect only affects the symmetric shapes; Linear is a 1D projection.
    ImGui::BeginDisabled(r->Type == 0);
    ImGui::Checkbox("Use window aspect ratio", &r->UseWindowAspect);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("On: round circles/squares (corrected for the window).\n"
                          "Off: 1:1 normalized space (stretched to the output).");

    const char* blendOpts[] = { "Replace", "Additive", "50/50" };
    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##blend", blendOpts[std::clamp(r->BlendMode, 0, 2)]))
    {
        for (int i = 0; i < 3; ++i)
            if (ImGui::Selectable(blendOpts[i], r->BlendMode == i)) r->BlendMode = i;
        ImGui::EndCombo();
    }

    // ── Gradient editor ──
    ImGui::SeparatorText("Gradient");
    ImGG::Settings settings;
    settings.gradient_width  = std::max(120.0f, ImGui::GetContentRegionAvail().x - 20.0f);
    settings.gradient_height = 32.0f;
    if (st.Widget.widget("##gradient", settings))
    {
        WidgetToStops(st.Widget, r->Stops);
        r->Bake();
    }

    if (ImGui::Button("Flip"))
    {
        for (RampStop& s : r->Stops) s.Position = 255 - s.Position;
        std::reverse(r->Stops.begin(), r->Stops.end());
        st.Widget = BuildWidget(r->Stops);
        r->Bake();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        r->Stops = { { 0, { 0, 0, 0 } }, { 255, { 255, 255, 255 } } };
        st.Widget = BuildWidget(r->Stops);
        r->Bake();
    }
}

void RegisterRampUI(ConfigUiRegistry& reg)
{
    reg.Register("Ramp", DrawRampUI);
}
