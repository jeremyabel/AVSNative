#pragma once

#include "engine/Reflect.h"

#include <array>
#include <cstdint>
#include <vector>

// Dot Fountain — a 3D particle fountain driven by the waveform. 256 generations × 30
// angular positions are spawned at the center, pushed upward, and pulled down by
// gravity while spreading outward; each particle projects to a single additive pixel.
// See ref/AVSWeb/src/effects/dot-fountain.js.

struct DotFountainConfig
{
    std::array<uint8_t, 3> Color0 = { 24, 107, 28 };
    std::array<uint8_t, 3> Color1 = { 35, 10, 255 };
    std::array<uint8_t, 3> Color2 = { 116, 29, 42 };
    std::array<uint8_t, 3> Color3 = { 217, 54, 144 };
    std::array<uint8_t, 3> Color4 = { 255, 136, 107 };
    int RotationSpeed = 16;   // -50..50
    int Angle         = -20;  // tilt degrees, -90..91
};

class DotFountain : public ReflectedEffect<DotFountainConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Color(&DotFountainConfig::Color0, "color0", "Color 1"),
            Color(&DotFountainConfig::Color1, "color1", "Color 2"),
            Color(&DotFountainConfig::Color2, "color2", "Color 3"),
            Color(&DotFountainConfig::Color3, "color3", "Color 4"),
            Color(&DotFountainConfig::Color4, "color4", "Color 5"),
            RangeI(&DotFountainConfig::RotationSpeed, "rotationSpeed", "Rotation Speed", -50, 50),
            RangeI(&DotFountainConfig::Angle,         "angle",         "Tilt Angle",     -90, 91),
        };
        return f;
    }
    std::string EffectName() const override { return "Dot Fountain"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override
    {
        for (const std::string& k : changed)
            if (k.rfind("color", 0) == 0) { BuildColorMap(); break; }
    }

private:
    static constexpr int NUM_GENS = 256;
    static constexpr int NUM_ANG  = 30;
    static constexpr int N        = NUM_GENS * NUM_ANG;

    void BuildColorMap();
    void EnsureOverlay(int w, int h);

    // Particle state (index = gen * NUM_ANG + ang).
    std::array<float, N>   m_rad{};
    std::array<float, N>   m_dRad{};
    std::array<float, N>   m_ht{};
    std::array<float, N>   m_dHt{};
    std::array<float, N>   m_ax{};
    std::array<float, N>   m_ay{};
    std::array<uint8_t, N> m_colR{};
    std::array<uint8_t, N> m_colG{};
    std::array<uint8_t, N> m_colB{};

    // 64-entry interpolated color map.
    std::array<uint8_t, 64> m_mapR{};
    std::array<uint8_t, 64> m_mapG{};
    std::array<uint8_t, 64> m_mapB{};

    float m_rotation = 0.0f;

    // CPU overlay buffer + bgfx texture.
    int                  m_overlayW = 0, m_overlayH = 0;
    std::vector<uint8_t> m_buf;
    bgfx::TextureHandle  m_overlayTex = BGFX_INVALID_HANDLE;

    // Composite (fs_simple, additive mode).
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uBase   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uOverlay = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uParams = BGFX_INVALID_HANDLE;
};
