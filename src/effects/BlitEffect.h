#pragma once

#include "engine/Reflect.h"

struct BlitEffectConfig
{
    float Zoom = 1.05f;
    float Rotation = 0.0f;
    float CenterX = 0.5f;
    float CenterY = 0.5f;
    // Matches the win32 Blitter Feedback default (bilinear off = nearest). With
    // this off, the zoom-feedback stays crisp like the original; bilinear softens
    // and blooms the buffer across frames.
    bool  Bilinear = false;
    // When bilinear is on, use the original AVS 8-bit integer 2x2 blend instead of
    // hardware bilinear (bit-exact match to win32). Ignored when Bilinear is off.
    bool  Compat   = false;
};

class BlitEffect : public ReflectedEffect<BlitEffectConfig>
{
public:
    
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Range(&BlitEffectConfig::Zoom, "zoom", "Zoom", 0.8f, 1.5f, 0.005f),
            Range(&BlitEffectConfig::Rotation, "rotation", "Rotation", -0.1f, 0.1f, 0.001f),
            Range(&BlitEffectConfig::CenterX, "centerX", "Center X", 0.0f, 1.0f, 0.01f),
            Range(&BlitEffectConfig::CenterY, "centerY", "Center Y", 0.0f, 1.0f, 0.01f),
            Bool(&BlitEffectConfig::Bilinear, "bilinear", "Bilinear"),
            Bool(&BlitEffectConfig::Compat, "bilinearCompat", "Bilinear (precise)"),
        };
        return f;
    }
    std::string EffectName() const override { return "Blit"; }

private:
    
    float Angle = 0.f;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle FlagsUniform = BGFX_INVALID_HANDLE;
};
