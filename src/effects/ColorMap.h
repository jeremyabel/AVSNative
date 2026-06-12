#pragma once

#include "engine/Effect.h"

#include <array>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

struct ColorMapStop
{
    int                  Position = 0;       // 0-255
    std::array<uint8_t,3> Color   = { 0, 0, 0 };
};

struct ColorMapEntry
{
    bool                      Enabled = false;
    std::vector<ColorMapStop> Stops   = { { 0,   { 0, 0, 0 } },
                                          { 255, { 255, 255, 255 } } };
};

class ColorMap : public Effect
{
public:
    static constexpr int kNumMaps = 8;
    static constexpr int kLutSize = 256;

    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  ColorKey          = 0;     // 0-5
    int  BlendMode         = 0;     // 0-9
    int  AdjustableAlpha   = 0;     // 0-255 (Adjustable blend)
    int  MapCycleMode      = 0;     // 0=None, 1=on-beat random, 2=on-beat sequential
    int  MapCycleSpeed     = 11;    // 1-64
    bool DontSkipFastBeats = false;
    int  CurrentMap        = 0;     // active/display map index

    // 8 maps, edited by the bespoke gradient UI.
    std::array<ColorMapEntry, kNumMaps> Maps;

    // Per-map keyboard binding (SDL keycode; 0 = unbound). Pressing a bound key
    // while the effect is enabled switches the displayed map to that index.
    std::array<uint32_t, kNumMaps> MapKeys{};

    static constexpr const char* kColorKey          = "colorKey";
    static constexpr const char* kBlendMode         = "blendmode";
    static constexpr const char* kAdjustableAlpha   = "adjustableAlpha";
    static constexpr const char* kMapCycleMode      = "mapCycleMode";
    static constexpr const char* kMapCycleSpeed     = "mapCycleSpeed";
    static constexpr const char* kDontSkipFastBeats = "dontSkipFastBeats";
    static constexpr const char* kCurrentMap        = "currentMap";
    static constexpr const char* kMaps              = "maps";
    static constexpr const char* kMapKeys           = "mapKeys";

    ColorMap() { Maps[0].Enabled = true; }

    void Init() override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    std::string Name() const override { return "Color Map"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Re-bake a single map's LUT from its stops (called by the UI after edits).
    void BakeMap(int idx);
    void BakeAll();

    // Incremented on Deserialize so the UI can resync its gradient widgets.
    uint64_t ConfigVersion() const { return m_configVersion; }

    // Bumped when a bound key switches the current map, so the UI can resync its
    // edit-selection radio to the newly displayed map.
    uint64_t KeySelectVersion() const { return m_keySelectVersion; }

private:
    bool        AnyEnabled() const;
    void        AdvanceNextMap();
    const uint8_t* SelectLUT(bool isBeat);   // returns 256*4 bytes

    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_lutUnif    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUnif = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_lutTex     = BGFX_INVALID_HANDLE;

    // CPU-side baked LUTs (RGBA8) and tween scratch.
    std::array<std::array<uint8_t, 256 * 4>, kNumMaps> m_baked{};
    std::array<uint8_t, 256 * 4>                       m_tween{};

    // Animation runtime state (not serialized).
    int m_changeStep = kLutSize;   // fully transitioned
    int m_nextMap    = 0;

    uint64_t m_configVersion    = 0;
    uint64_t m_keySelectVersion = 0;
};
