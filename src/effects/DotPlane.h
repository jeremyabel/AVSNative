#pragma once

#include "engine/Effect.h"

#include <array>
#include <cstdint>
#include <vector>

// Dot Plane — a 64×64 grid of dots forming an undulating plane. Each frame the grid
// scrolls back, decaying in height, while row 0 takes new spectrum data; the plane is
// rotated/tilted in 3D and each grid point projects to a single dot.
// See ref/AVSWeb/src/effects/dot-plane.js.

class DotPlane : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int RotationSpeed = 16;   // -50..50
    int Angle         = -20;  // -90..91
    std::array<uint8_t, 3> Color0 = { 28, 107, 24 };
    std::array<uint8_t, 3> Color1 = { 255, 10, 35 };
    std::array<uint8_t, 3> Color2 = { 42, 29, 116 };
    std::array<uint8_t, 3> Color3 = { 144, 54, 217 };
    std::array<uint8_t, 3> Color4 = { 107, 136, 255 };

    static constexpr const char* kRotationSpeed = "rotationSpeed";
    static constexpr const char* kAngle         = "angle";
    static constexpr const char* kColor0        = "color0";
    static constexpr const char* kColor1        = "color1";
    static constexpr const char* kColor2        = "color2";
    static constexpr const char* kColor3        = "color3";
    static constexpr const char* kColor4        = "color4";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Dot Plane"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Rebuilds the 64-entry interpolated color map from Color0..Color4. Called
    // after Deserialize and by the UI when any color changes.
    void BuildColorMap();

private:
    static constexpr int GRID = 64;

    void EnsureDots(int w, int h);
    void UpdateGrid(const struct VisData* vd);

    // Grid state (index = row * GRID + col).
    std::array<float, GRID * GRID>   m_height{};
    std::array<float, GRID * GRID>   m_delta{};
    std::array<uint8_t, GRID * GRID> m_color{};
    std::array<float, GRID>          m_tmp{};

    std::array<uint8_t, 64 * 3> m_colorMap{};

    float m_rotation = 0.0f;

    // CPU dots buffer + bgfx texture.
    int                  m_dotsW = 0, m_dotsH = 0;
    std::vector<uint8_t> m_buf;
    bgfx::TextureHandle  m_dotsTex = BGFX_INVALID_HANDLE;

    // Composite (fs_dotplane, screen blend).
    bgfx::ProgramHandle m_program  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uInput   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uDots    = BGFX_INVALID_HANDLE;
};
