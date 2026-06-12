#pragma once

#include "engine/Effect.h"

#include <array>
#include <cstdint>
#include <vector>

// Dot Fountain — a 3D particle fountain driven by the waveform. 256 generations × 30
// angular positions are spawned at the center, pushed upward, and pulled down by
// gravity while spreading outward; each particle projects to a single additive pixel.
// See ref/AVSWeb/src/effects/dot-fountain.js.

class DotFountain : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::array<uint8_t, 3> Color0 = { 24, 107, 28 };
    std::array<uint8_t, 3> Color1 = { 35, 10, 255 };
    std::array<uint8_t, 3> Color2 = { 116, 29, 42 };
    std::array<uint8_t, 3> Color3 = { 217, 54, 144 };
    std::array<uint8_t, 3> Color4 = { 255, 136, 107 };
    int RotationSpeed = 16;   // -50..50
    int Angle         = -20;  // tilt degrees, -90..91

    static constexpr const char* kColor0        = "color0";
    static constexpr const char* kColor1        = "color1";
    static constexpr const char* kColor2        = "color2";
    static constexpr const char* kColor3        = "color3";
    static constexpr const char* kColor4        = "color4";
    static constexpr const char* kRotationSpeed = "rotationSpeed";
    static constexpr const char* kAngle         = "angle";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Dot Fountain"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Rebuilds the 64-entry interpolated color map from Color0..Color4. Called
    // after Deserialize and by the UI when any color changes.
    void BuildColorMap();

private:
    static constexpr int NUM_GENS = 256;
    static constexpr int NUM_ANG  = 30;
    static constexpr int N        = NUM_GENS * NUM_ANG;

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
