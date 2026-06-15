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

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Dot Fountain"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    void BuildColorMap();

public:

    int RotationSpeed = 16;
    int Angle = -20;
    std::array<uint8_t, 3> Color0 = { 24, 107, 28 };
    std::array<uint8_t, 3> Color1 = { 35, 10, 255 };
    std::array<uint8_t, 3> Color2 = { 116, 29, 42 };
    std::array<uint8_t, 3> Color3 = { 217, 54, 144 };
    std::array<uint8_t, 3> Color4 = { 255, 136, 107 };

private:

    static constexpr int NUM_GENS = 256;
    static constexpr int NUM_ANG = 30;
    static constexpr int N = NUM_GENS * NUM_ANG;

    void EnsureOverlay(int w, int h);

    // Particle state (index = gen * NUM_ANG + ang).
    std::array<float, N> m_rad{};
    std::array<float, N> m_dRad{};
    std::array<float, N> m_ht{};
    std::array<float, N> m_dHt{};
    std::array<float, N> m_ax{};
    std::array<float, N> m_ay{};
    std::array<uint8_t, N> m_colR{};
    std::array<uint8_t, N> m_colG{};
    std::array<uint8_t, N> m_colB{};

    // 64-entry interpolated color map.
    std::array<uint8_t, 64> m_mapR{};
    std::array<uint8_t, 64> m_mapG{};
    std::array<uint8_t, 64> m_mapB{};

    float CurrentRotation = 0.0f;

    // CPU overlay buffer + bgfx texture.
    int OverlayW = 0; 
    int OverlayH = 0;
    std::vector<uint8_t> OverlayBuffer;
    bgfx::TextureHandle  OverlayTexture = BGFX_INVALID_HANDLE;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle OverlayUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
