#pragma once

#include "engine/Reflect.h"

struct RotoBlitterConfig
{
    int  ZoomScale   = 31;    // 0-256; 31 = no zoom
    int  ZoomScale2  = 31;    // beat zoom target
    int  RotDir      = 31;    // 0-64; 32 = no rotation, <32 one way, >32 other
    int  BeatchSpeed = 0;     // 0-8; rotation reversal smoothing
    bool Subpixel    = true;  // hardware bilinear (vs nearest)
    bool Compat      = false; // 8-bit integer bilinear matching the win32 original
    bool Blend       = false;
    bool Beatch      = false; // reverse rotation on beat
    bool BeatchScale = false; // snap zoom on beat
};

class RotoBlitter : public ReflectedEffect<RotoBlitterConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI(&RotoBlitterConfig::ZoomScale, "zoom_scale", "Zoom", 0, 256),
            RangeI(&RotoBlitterConfig::ZoomScale2, "zoom_scale2", "Zoom (On Beat)", 0, 256),
            RangeI(&RotoBlitterConfig::RotDir, "rot_dir", "Rotation", 0, 64),
            RangeI(&RotoBlitterConfig::BeatchSpeed, "beatch_speed", "Reversal Smoothing", 0, 8),
            Bool(&RotoBlitterConfig::Subpixel, "subpixel", "Subpixel"),
            Bool(&RotoBlitterConfig::Compat, "bilinearCompat", "Bilinear (precise)"),
            Bool(&RotoBlitterConfig::Blend, "blend", "Blend"),
            Bool(&RotoBlitterConfig::Beatch, "beatch", "Reverse on Beat"),
            Bool(&RotoBlitterConfig::BeatchScale, "beatch_scale", "Zoom Snap on Beat"),
        };
        return f;
    }
    std::string EffectName() const override { return "Roto Blitter"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override
    {
        for (const std::string& k : changed)
            if (k == "zoom_scale")
                m_scaleFpos = (float)Cfg.ZoomScale; // reset animation on load
    }

private:
    // Runtime state (not serialized)
    float m_rotRev    =  1.0f; // target: 1 or -1
    float m_rotRevPos =  1.0f; // smoothed rotRev
    float m_scaleFpos = 31.0f; // animates toward ZoomScale

    bgfx::ProgramHandle Program          = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TransformUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ResolutionUniform = BGFX_INVALID_HANDLE;
};
