#pragma once

#include "engine/Reflect.h"

#include <array>
#include <cstdint>
#include <vector>

// Dot Plane — a 64×64 grid of dots forming an undulating plane. Each frame the grid
// scrolls back, decaying in height, while row 0 takes new spectrum data; the plane is
// rotated/tilted in 3D and each grid point projects to a single dot.
// See ref/AVSWeb/src/effects/dot-plane.js.

struct DotPlaneConfig
{
    int RotationSpeed = 16;   // -50..50
    int Angle         = -20;  // -90..91
    std::array<uint8_t, 3> Color0 = { 28, 107, 24 };
    std::array<uint8_t, 3> Color1 = { 255, 10, 35 };
    std::array<uint8_t, 3> Color2 = { 42, 29, 116 };
    std::array<uint8_t, 3> Color3 = { 144, 54, 217 };
    std::array<uint8_t, 3> Color4 = { 107, 136, 255 };
};

class DotPlane : public ReflectedEffect<DotPlaneConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI(&DotPlaneConfig::RotationSpeed, "rotationSpeed", "Rotation Speed", -50, 50),
            RangeI(&DotPlaneConfig::Angle,         "angle",         "Angle",          -90, 91),
            Color(&DotPlaneConfig::Color0, "color0", "Color 1"),
            Color(&DotPlaneConfig::Color1, "color1", "Color 2"),
            Color(&DotPlaneConfig::Color2, "color2", "Color 3"),
            Color(&DotPlaneConfig::Color3, "color3", "Color 4"),
            Color(&DotPlaneConfig::Color4, "color4", "Color 5"),
        };
        return f;
    }
    std::string EffectName() const override { return "Dot Plane"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override
    {
        for (const std::string& k : changed)
            if (k.rfind("color", 0) == 0) { BuildColorMap(); break; }
    }

private:
    static constexpr int GRID = 64;

    void BuildColorMap();
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
