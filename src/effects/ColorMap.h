#pragma once

#include "engine/Reflect.h"

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

struct ColorMapConfig
{
    int  ColorKey          = 0;     // 0-5
    int  BlendMode         = 0;     // 0-9
    int  AdjustableAlpha   = 0;     // 0-255 (Adjustable blend)
    int  MapCycleMode      = 0;     // 0=None, 1=on-beat random, 2=on-beat sequential
    int  MapCycleSpeed     = 11;    // 1-64
    bool DontSkipFastBeats = false;
    int  CurrentMap        = 0;     // active/display map index

    // 8 maps managed manually (not reflected).
    std::array<ColorMapEntry, 8> Maps;

    ColorMapConfig() { Maps[0].Enabled = true; }
};

class ColorMap : public ReflectedEffect<ColorMapConfig>
{
public:
    static constexpr int kNumMaps = 8;
    static constexpr int kLutSize = 256;

    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    nlohmann::json GetConfig() const override;
    void           SetConfig(const nlohmann::json& cfg) override;

    // Re-bake a single map's LUT from its stops (called by the UI after edits).
    void BakeMap(int idx);
    void BakeAll();

    // Incremented on external SetConfig so the UI can resync its gradient widgets.
    uint64_t ConfigVersion() const { return m_configVersion; }

protected:
    const std::vector<Field>& Fields() const override;
    std::string EffectName() const override { return "Color Map"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

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

    uint64_t m_configVersion = 0;
};
