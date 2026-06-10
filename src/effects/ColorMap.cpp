#include "effects/ColorMap.h"
#include "engine/FBOManager.h"

#include <bgfx/bgfx.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_colormap.sc.bin.h"

#include <algorithm>
#include <cstdlib>

// ── Baking ─────────────────────────────────────────────────────────────────────

void ColorMap::BakeMap(int idx)
{
    if (idx < 0 || idx >= kNumMaps) return;

    std::vector<ColorMapStop> sorted = Cfg.Maps[idx].Stops;
    if (sorted.empty()) return;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const ColorMapStop& a, const ColorMapStop& b) {
                         if (a.Position != b.Position) return a.Position < b.Position;
                         return (a.Color[0] + a.Color[1] + a.Color[2])
                              < (b.Color[0] + b.Color[1] + b.Color[2]);
                     });

    uint8_t* lut = m_baked[idx].data();

    const ColorMapStop& first = sorted.front();
    const int firstPos = std::clamp(first.Position, 0, kLutSize);
    for (int i = 0; i < firstPos; ++i) {
        lut[i*4+0] = first.Color[0];
        lut[i*4+1] = first.Color[1];
        lut[i*4+2] = first.Color[2];
        lut[i*4+3] = 255;
    }

    for (size_t ci = 0; ci + 1 < sorted.size(); ++ci) {
        const ColorMapStop& from = sorted[ci];
        const ColorMapStop& to   = sorted[ci + 1];
        const int fp = std::clamp(from.Position, 0, kLutSize);
        const int tp = std::clamp(to.Position,   0, kLutSize);
        const int span = tp - fp;
        for (int i = fp; i < tp; ++i) {
            const float t = span > 0 ? (float)(i - fp) / (float)span : 0.0f;
            lut[i*4+0] = (uint8_t)(from.Color[0] + (to.Color[0] - from.Color[0]) * t);
            lut[i*4+1] = (uint8_t)(from.Color[1] + (to.Color[1] - from.Color[1]) * t);
            lut[i*4+2] = (uint8_t)(from.Color[2] + (to.Color[2] - from.Color[2]) * t);
            lut[i*4+3] = 255;
        }
    }

    const ColorMapStop& last = sorted.back();
    const int lastPos = std::clamp(last.Position, 0, kLutSize);
    for (int i = lastPos; i < kLutSize; ++i) {
        lut[i*4+0] = last.Color[0];
        lut[i*4+1] = last.Color[1];
        lut[i*4+2] = last.Color[2];
        lut[i*4+3] = 255;
    }
}

void ColorMap::BakeAll()
{
    for (int i = 0; i < kNumMaps; ++i) BakeMap(i);
}

// ── GetConfig / SetConfig ─────────────────────────────────────────────────────

nlohmann::json ColorMap::GetConfig() const
{
    nlohmann::json j = ReflectedEffect<ColorMapConfig>::GetConfig();
    j["currentMap"] = Cfg.CurrentMap;

    nlohmann::json maps = nlohmann::json::array();
    for (const ColorMapEntry& m : Cfg.Maps) {
        nlohmann::json colors = nlohmann::json::array();
        for (const ColorMapStop& s : m.Stops)
            colors.push_back({ { "position", s.Position },
                               { "color", { s.Color[0], s.Color[1], s.Color[2] } } });
        maps.push_back({ { "enabled", m.Enabled }, { "colors", colors } });
    }
    j["maps"] = maps;
    return j;
}

void ColorMap::SetConfig(const nlohmann::json& cfg)
{
    ReflectedEffect<ColorMapConfig>::SetConfig(cfg);

    if (cfg.contains("currentMap") && cfg["currentMap"].is_number())
        Cfg.CurrentMap = std::clamp(cfg["currentMap"].get<int>(), 0, kNumMaps - 1);

    if (cfg.contains("maps") && cfg["maps"].is_array()) {
        const auto& maps = cfg["maps"];
        for (int i = 0; i < kNumMaps && i < (int)maps.size(); ++i) {
            const auto& m = maps[i];
            Cfg.Maps[i].Enabled = m.value("enabled", false);
            if (m.contains("colors") && m["colors"].is_array()) {
                std::vector<ColorMapStop> stops;
                for (const auto& c : m["colors"]) {
                    ColorMapStop s;
                    s.Position = std::clamp(c.value("position", 0), 0, 255);
                    const auto& col = c["color"];
                    if (col.is_array() && col.size() == 3)
                        s.Color = { (uint8_t)std::clamp(col[0].get<int>(), 0, 255),
                                    (uint8_t)std::clamp(col[1].get<int>(), 0, 255),
                                    (uint8_t)std::clamp(col[2].get<int>(), 0, 255) };
                    stops.push_back(s);
                }
                if (!stops.empty())
                    Cfg.Maps[i].Stops = std::move(stops);
            }
        }
    }

    m_nextMap = Cfg.CurrentMap;
    BakeAll();
    ++m_configVersion;
}

void ColorMap::OnConfigChanged(const std::vector<std::string>& changed)
{
    // The UI edits Maps directly then notifies with "maps"; re-bake on any such change.
    if (std::find(changed.begin(), changed.end(), "maps") != changed.end())
        BakeAll();
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void ColorMap::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_colormap_spv,   sizeof(fs_colormap_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif  = bgfx::createUniform("s_input",    bgfx::UniformType::Sampler);
    m_lutUnif    = bgfx::createUniform("s_lut",      bgfx::UniformType::Sampler);
    m_paramsUnif = bgfx::createUniform("u_cmParams", bgfx::UniformType::Vec4);

    m_lutTex = bgfx::createTexture2D(
        kLutSize, 1, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
        BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP);

    BakeAll();
}

void ColorMap::Destroy()
{
    if (bgfx::isValid(m_lutTex))     bgfx::destroy(m_lutTex);
    if (bgfx::isValid(m_paramsUnif)) bgfx::destroy(m_paramsUnif);
    if (bgfx::isValid(m_lutUnif))    bgfx::destroy(m_lutUnif);
    if (bgfx::isValid(m_inputUnif))  bgfx::destroy(m_inputUnif);
    if (bgfx::isValid(m_prog))       bgfx::destroy(m_prog);

    m_lutTex     = BGFX_INVALID_HANDLE;
    m_paramsUnif = BGFX_INVALID_HANDLE;
    m_lutUnif    = BGFX_INVALID_HANDLE;
    m_inputUnif  = BGFX_INVALID_HANDLE;
    m_prog       = BGFX_INVALID_HANDLE;
}

// ── Map cycling ────────────────────────────────────────────────────────────────

bool ColorMap::AnyEnabled() const
{
    for (const ColorMapEntry& m : Cfg.Maps)
        if (m.Enabled) return true;
    return false;
}

void ColorMap::AdvanceNextMap()
{
    if (!AnyEnabled()) return;
    const int start = m_nextMap;
    do {
        if (Cfg.MapCycleMode == 1)
            m_nextMap = std::rand() % kNumMaps;
        else
            m_nextMap = (m_nextMap + 1) % kNumMaps;
    } while (!Cfg.Maps[m_nextMap].Enabled && m_nextMap != start);
}

const uint8_t* ColorMap::SelectLUT(bool isBeat)
{
    const int cur = std::clamp(Cfg.CurrentMap, 0, kNumMaps - 1);

    if (Cfg.MapCycleMode == 0) {
        m_changeStep = 0;
        return m_baked[cur].data();
    }

    m_changeStep = std::min(m_changeStep + Cfg.MapCycleSpeed, kLutSize);

    if (isBeat && (!Cfg.DontSkipFastBeats || m_changeStep == kLutSize)) {
        AdvanceNextMap();
        m_changeStep = 0;
    }

    if (m_changeStep == 0)
        return m_baked[cur].data();

    if (m_changeStep >= kLutSize) {
        Cfg.CurrentMap = m_nextMap;
        return m_baked[m_nextMap].data();
    }

    if (cur == m_nextMap)
        return m_baked[cur].data();

    const float t = (float)m_changeStep / (float)kLutSize;
    const uint8_t* a = m_baked[cur].data();
    const uint8_t* b = m_baked[m_nextMap].data();
    for (int i = 0; i < kLutSize * 4; i += 4) {
        m_tween[i+0] = (uint8_t)(a[i+0] + (b[i+0] - a[i+0]) * t);
        m_tween[i+1] = (uint8_t)(a[i+1] + (b[i+1] - a[i+1]) * t);
        m_tween[i+2] = (uint8_t)(a[i+2] + (b[i+2] - a[i+2]) * t);
        m_tween[i+3] = 255;
    }
    return m_tween.data();
}

// ── Render ────────────────────────────────────────────────────────────────────

void ColorMap::Render(const RenderContext& Ctx)
{
    const uint8_t* lut = SelectLUT(Ctx.IsBeat());
    bgfx::updateTexture2D(m_lutTex, 0, 0, 0, 0, kLutSize, 1,
                          bgfx::copy(lut, kLutSize * 4));

    float params[4] = {
        (float)Cfg.ColorKey,
        (float)Cfg.BlendMode,
        (float)Cfg.AdjustableAlpha / 255.0f,
        0.0f,
    };

    bgfx::setTexture(0, m_inputUnif, Ctx.InputTexture);
    bgfx::setTexture(1, m_lutUnif,   m_lutTex);
    bgfx::setUniform(m_paramsUnif, params);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Ctx.QuadVB);
    bgfx::submit(Ctx.ViewId, m_prog);

    Ctx.FboManager->Swap();
}
