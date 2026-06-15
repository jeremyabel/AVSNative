#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"
#include "ui/FileDialog.h"

#include "effects/ColorMap.h"

#include <imgui.h>
#include <imgui_gradient/imgui_gradient.hpp>
#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

// ── Per-effect editor state (gradient widgets persist across frames) ───────────

struct ColorMapUiState
{
    int      Selected            = 0;
    uint64_t LastVersion         = (uint64_t)-1;   // force initial resync
    uint64_t LastKeySelectVersion = (uint64_t)-1;  // resync radio to key-chosen map
    std::array<ImGG::GradientWidget, ColorMap::kNumMaps> Widgets;
};

static std::unordered_map<Effect*, ColorMapUiState> g_states;

// ── Conversions between our stops and the library's marks ──────────────────────

static ImGG::GradientWidget BuildWidget(const ColorMapEntry& map)
{
    std::list<ImGG::Mark> marks;
    for (const ColorMapStop& s : map.Stops)
    {
        const float pos = std::clamp(s.Position, 0, 255) / 255.0f;
        marks.push_back(ImGG::Mark{
            ImGG::RelativePosition{ pos },
            ImGG::ColorRGBA{ s.Color[0] / 255.0f, s.Color[1] / 255.0f,
                             s.Color[2] / 255.0f, 1.0f } });
    }
    return ImGG::GradientWidget{ marks };
}

static void WidgetToStops(const ImGG::GradientWidget& w, ColorMapEntry& map)
{
    std::vector<ColorMapStop> stops;
    for (const ImGG::Mark& m : w.gradient().get_marks())
    {
        ColorMapStop s;
        s.Position = std::clamp((int)std::lround(m.position.get() * 255.0f), 0, 255);
        s.Color = {
            (uint8_t)std::clamp((int)std::lround(m.color.x * 255.0f), 0, 255),
            (uint8_t)std::clamp((int)std::lround(m.color.y * 255.0f), 0, 255),
            (uint8_t)std::clamp((int)std::lround(m.color.z * 255.0f), 0, 255),
        };
        stops.push_back(s);
    }
    if (!stops.empty())
        map.Stops = std::move(stops);
}

// ── CLM file save / load (matches the reference's CLM1 format) ─────────────────

static void WriteU32(std::ofstream& f, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)(v & 0xff), (uint8_t)((v >> 8) & 0xff),
                     (uint8_t)((v >> 16) & 0xff), (uint8_t)((v >> 24) & 0xff) };
    f.write((const char*)b, 4);
}
static uint32_t ReadU32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static const SDL_DialogFileFilter kClmFilters[] = {
    { "Color Map (.clm)", "clm" },
    { "All Files",        "*"   },
};

static void SaveClm(const ColorMapEntry& map, int idx)
{
    char defName[16];
    snprintf(defName, sizeof(defName), "map%d.clm", idx + 1);
    const std::string path = FileDialog::Save("Save Color Map", defName, kClmFilters, 2);
    if (path.empty()) return;

    std::ofstream f(path, std::ios::binary);
    if (!f) return;

    std::vector<ColorMapStop> colors = map.Stops;
    if (colors.size() > 256) colors.resize(256);

    const char magic[4] = { 'C', 'L', 'M', '1' };
    f.write(magic, 4);
    WriteU32(f, (uint32_t)colors.size());
    for (const ColorMapStop& s : colors)
    {
        const uint32_t packed = ((uint32_t)s.Color[0] << 16) |
                                ((uint32_t)s.Color[1] << 8) | (uint32_t)s.Color[2];
        WriteU32(f, (uint32_t)s.Position);
        WriteU32(f, packed);
        WriteU32(f, 0);   // color_id — ignored on load
    }
}

static bool LoadClm(ColorMapEntry& map)
{
    const std::string path = FileDialog::Open("Load Color Map", kClmFilters, 2);
    if (path.empty()) return false;

    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    if (buf.size() < 8) return false;
    if (buf[0] != 'C' || buf[1] != 'L' || buf[2] != 'M' || buf[3] != '1') return false;

    const int COLOR_SIZE = 12;
    const int MAX_COLORS = 256;
    uint32_t length    = ReadU32(buf.data() + 4);
    const int available = (int)((buf.size() - 8) / COLOR_SIZE);
    if ((int)length > MAX_COLORS || (int)length != available)
        length = (uint32_t)std::min(available, MAX_COLORS);

    std::vector<ColorMapStop> stops;
    size_t off = 8;
    for (uint32_t i = 0; i < length; ++i)
    {
        if (off + COLOR_SIZE > buf.size()) break;
        const uint32_t position = ReadU32(buf.data() + off); off += 4;
        const uint32_t packed   = ReadU32(buf.data() + off); off += 4;
        off += 4;   // skip color_id
        ColorMapStop s;
        s.Position = std::clamp((int)position, 0, 255);
        s.Color = { (uint8_t)((packed >> 16) & 0xff),
                    (uint8_t)((packed >> 8) & 0xff),
                    (uint8_t)(packed & 0xff) };
        stops.push_back(s);
    }
    if (stops.empty()) return false;
    map.Stops = std::move(stops);
    return true;
}

// ── Bespoke UI ─────────────────────────────────────────────────────────────────

static void DrawColorMapUI(Effect* effect)
{
    auto* cm = static_cast<ColorMap*>(effect);
    ColorMapUiState& st = g_states[effect];

    // Resync gradient widgets if the config changed externally (e.g. preset load).
    if (st.LastVersion != cm->ConfigVersion())
    {
        for (int i = 0; i < ColorMap::kNumMaps; ++i)
            st.Widgets[i] = BuildWidget(cm->Maps[i]);
        st.Selected    = std::clamp(cm->CurrentMap, 0, ColorMap::kNumMaps - 1);
        st.LastVersion = cm->ConfigVersion();
    }

    // Follow the displayed map onto the edit radio when a bound key switched it.
    if (st.LastKeySelectVersion != cm->KeySelectVersion())
    {
        st.Selected = std::clamp(cm->CurrentMap, 0, ColorMap::kNumMaps - 1);
        st.LastKeySelectVersion = cm->KeySelectVersion();
    }

    // ── Map slots: enable checkbox + select radio + key binding ──
    ImGui::TextUnformatted("Maps");
    for (int i = 0; i < ColorMap::kNumMaps; ++i)
    {
        ImGui::PushID(i);
        bool en = cm->Maps[i].Enabled;
        if (ImGui::Checkbox("##en", &en))
            cm->Maps[i].Enabled = en;
        ImGui::SameLine();
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "Map %d", i + 1);
        if (ImGui::RadioButton(lbl, st.Selected == i))
            st.Selected = i;
        ImGui::SameLine(110.0f);
        ConfigUi::KeyCaptureButton("##key", cm, i, cm->MapKeys[i]);
        ImGui::PopID();
    }

    const int sel = std::clamp(st.Selected, 0, ColorMap::kNumMaps - 1);

    // When not cycling, display the map being edited.
    if (cm->MapCycleMode == 0)
        cm->CurrentMap = sel;

    // ── Gradient editor for the selected map ──
    ImGui::SeparatorText("Gradient");
    ImGG::Settings settings;
    settings.gradient_width = std::max(120.0f, ImGui::GetContentRegionAvail().x - 20.0f);
    settings.gradient_height = 32.0f;
    if (st.Widgets[sel].widget("##gradient", settings))
    {
        WidgetToStops(st.Widgets[sel], cm->Maps[sel]);
        cm->BakeMap(sel);
    }

    // ── Per-map actions ──
    if (ImGui::Button("Flip"))
    {
        auto& stops = cm->Maps[sel].Stops;
        for (ColorMapStop& s : stops) s.Position = 255 - s.Position;
        std::reverse(stops.begin(), stops.end());
        st.Widgets[sel] = BuildWidget(cm->Maps[sel]);
        cm->BakeMap(sel);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        cm->Maps[sel].Stops = { { 0, { 0, 0, 0 } }, { 255, { 255, 255, 255 } } };
        st.Widgets[sel] = BuildWidget(cm->Maps[sel]);
        cm->BakeMap(sel);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save .clm"))
        SaveClm(cm->Maps[sel], sel);
    ImGui::SameLine();
    if (ImGui::Button("Load .clm"))
    {
        if (LoadClm(cm->Maps[sel]))
        {
            st.Widgets[sel] = BuildWidget(cm->Maps[sel]);
            cm->BakeMap(sel);
        }
    }

    // ── Scalar parameters ──
    ImGui::SeparatorText("Settings");

    const char* keyOpts[] = { "Red Channel", "Green Channel", "Blue Channel",
                              "(R+G+B)/2", "Maximal Channel", "(R+G+B)/3" };
    ImGui::TextUnformatted("Key");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##key", keyOpts[std::clamp(cm->ColorKey, 0, 5)]))
    {
        for (int i = 0; i < 6; ++i)
        {
            if (ImGui::Selectable(keyOpts[i], cm->ColorKey == i)) 
                cm->ColorKey = i;
        }
        ImGui::EndCombo();
    }

    const char* blendOpts[] = { "Replace", "Additive", "Maximum", "Minimum", "50/50",
                                "Subtractive 1", "Subtractive 2", "Multiply", "XOR", "Adjustable" };
    ImGui::TextUnformatted("Blend Mode");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##blend", blendOpts[std::clamp(cm->BlendMode, 0, 9)]))
    {
        for (int i = 0; i < 10; ++i)
        {
            if (ImGui::Selectable(blendOpts[i], cm->BlendMode == i)) 
                cm->BlendMode = i;
        }
        ImGui::EndCombo();
    }

    if (cm->BlendMode == 9) // Adjustable
    {
        ImGui::TextUnformatted("Alpha");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##alpha", &cm->AdjustableAlpha, 0, 255);
    }

    const char* cycleOpts[] = { "None (single map)", "On-beat random", "On-beat sequential" };
    ImGui::TextUnformatted("Cycling");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##cycle", cycleOpts[std::clamp(cm->MapCycleMode, 0, 2)]))
    {
        for (int i = 0; i < 3; ++i)
        {
            if (ImGui::Selectable(cycleOpts[i], cm->MapCycleMode == i)) 
                cm->MapCycleMode = i;
        }
        ImGui::EndCombo();
    }

    if (cm->MapCycleMode != 0)
    {
        ImGui::TextUnformatted("Cycle Speed");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##speed", &cm->MapCycleSpeed, 1, 64);
        ImGui::Checkbox("Don't Skip Fast Beats", &cm->DontSkipFastBeats);
    }
}

void RegisterColorMapUI(ConfigUiRegistry& reg)
{
    reg.Register("Color Map", DrawColorMapUI);
}
