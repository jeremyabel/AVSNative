#pragma once

#include "engine/Reflect.h"

// Mosaic — pixelates the image into an n×n grid of blocks. Faithful to the original
// vis_avs e_mosaic.cpp: `Size` is the mosaic resolution (blocks across, 1..100; 100 =
// no mosaic, not a pixel block size), with optional on-beat size change that eases back
// over a cooldown, plus a blend mode combining the mosaic with the original pixel.

struct MosaicConfig
{
    int  Size             = 50;     // 1..100 blocks across (100 = no mosaic)
    bool OnBeatSizeChange = false;
    int  OnBeatSize       = 50;     // 1..100
    int  OnBeatDuration   = 15;     // 1..100 frames to ease back to Size
    int  Blend            = 0;      // 0 = Replace, 1 = Additive, 2 = 50/50
};

class Mosaic : public ReflectedEffect<MosaicConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI(&MosaicConfig::Size, "size", "Size", 1, 100),
            Bool(&MosaicConfig::OnBeatSizeChange, "onBeatSizeChange", "On-Beat Size Change"),
            RangeI(&MosaicConfig::OnBeatSize,     "onBeatSize",     "On-Beat Size", 1, 100),
            RangeI(&MosaicConfig::OnBeatDuration, "onBeatDuration", "On-Beat Duration", 1, 100),
            SelectI(&MosaicConfig::Blend, "blend", "Blend", { "Replace", "Additive", "50/50" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Mosaic"; }

private:
    // Runtime on-beat state.
    int m_curSize  = 50;
    int m_cooldown = 0;

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
