#pragma once

#include "engine/Effect.h"

#include <array>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

// One gradient color stop, mirroring Color Map's stop model (position 0-255 + RGB).
struct RampStop
{
    int                   Position = 0;          // 0-255
    std::array<uint8_t,3> Color    = { 0, 0, 0 };
};

// Draws a gradient (linear / radial / diamond / square) at a given scale and
// rotation, baked from a Color-Map-style stop list into a 256-entry LUT and
// sampled per pixel. Optionally blends over the incoming buffer.
class Ramp : public Effect
{
public:
    static constexpr int kLutSize = 256;

    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int   Type      = 0;      // 0=Linear, 1=Radial, 2=Diamond, 3=Square
    float Scale     = 1.0f;   // gradient zoom (>1 compresses toward center)
    float Rotation  = 0.0f;   // degrees
    int   BlendMode = 0;      // 0=Replace, 1=Additive, 2=50/50
    // Aspect handling for the symmetric shapes: true = correct for the window's
    // aspect (round circles/squares), false = compute in 1:1 normalized space
    // (stretched to the output). No effect on Linear (a 1D projection).
    bool  UseWindowAspect = true;

    std::vector<RampStop> Stops = { { 0,   { 0, 0, 0 } },
                                    { 255, { 255, 255, 255 } } };

    static constexpr const char* kType            = "type";
    static constexpr const char* kScale           = "scale";
    static constexpr const char* kRotation        = "rotation";
    static constexpr const char* kBlendMode       = "blendmode";
    static constexpr const char* kUseWindowAspect = "useWindowAspect";
    static constexpr const char* kColors          = "colors";

    void Init() override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    std::string Name() const override { return "Ramp"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Re-bake the LUT from the current stops (called by the UI after edits).
    void Bake();

    // Incremented on Deserialize so the UI can resync its gradient widget.
    uint64_t ConfigVersion() const { return m_configVersion; }

private:
    bgfx::ProgramHandle m_prog        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_lutUnif     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUnif2 = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_lutTex      = BGFX_INVALID_HANDLE;

    std::array<uint8_t, kLutSize * 4> m_baked{};

    uint64_t m_configVersion = 0;
};
