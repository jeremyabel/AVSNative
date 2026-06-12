#pragma once

#include "engine/Effect.h"

// Mosaic — pixelates the image into an n×n grid of blocks. Faithful to the original
// vis_avs e_mosaic.cpp: `Size` is the mosaic resolution (blocks across, 1..100; 100 =
// no mosaic, not a pixel block size), with optional on-beat size change that eases back
// over a cooldown, plus a blend mode combining the mosaic with the original pixel.

class Mosaic : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  Size             = 50;     // 1..100 blocks across (100 = no mosaic)
    bool OnBeatSizeChange = false;
    int  OnBeatSize       = 50;     // 1..100
    int  OnBeatDuration   = 15;     // 1..100 frames to ease back to Size
    int  Blend            = 0;      // 0 = Replace, 1 = Additive, 2 = 50/50

    static constexpr const char* kSize             = "size";
    static constexpr const char* kOnBeatSizeChange = "onBeatSizeChange";
    static constexpr const char* kOnBeatSize       = "onBeatSize";
    static constexpr const char* kOnBeatDuration   = "onBeatDuration";
    static constexpr const char* kBlend            = "blend";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Mosaic"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // Runtime on-beat state.
    int m_curSize  = 50;
    int m_cooldown = 0;

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
