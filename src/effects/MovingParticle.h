#pragma once

#include "engine/Reflect.h"

struct MovingParticleConfig
{
    // Defaults match original AVS
    std::array<uint8_t, 3> Color           = { 255, 255, 255 };
    int  Distance         = 16;   // 1..32
    int  Size             = 8;    // 1..128
    bool OnBeatSizeChange = false;
    int  OnBeatSize       = 8;    // 1..128
    int  BlendMode        = 1;    // 0=Replace 1=Additive 2=50/50 3=Default
};

class MovingParticle : public ReflectedEffect<MovingParticleConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Color(&MovingParticleConfig::Color, "color", "Color"),
            RangeI(&MovingParticleConfig::Distance, "distance", "Distance", 1, 32),
            RangeI(&MovingParticleConfig::Size, "size", "Size", 1, 128),
            Bool(&MovingParticleConfig::OnBeatSizeChange, "onBeatSizeChange", "On Beat Size Change"),
            RangeI(&MovingParticleConfig::OnBeatSize, "onBeatSize", "On Beat Size", 1, 128),
            SelectI(&MovingParticleConfig::BlendMode, "blendMode", "Blend Mode",
                    { "Replace", "Additive", "50/50", "Default" }),
        };
        return f;
    }
    std::string EffectName() const override { return "MovingParticle"; }

private:
    // Physics state — exact initial values from AVS_Remake
    float AttractorX = 0.0f,      AttractorY = 0.0f;
    float VelX       = -0.01551f, VelY       = 0.0f;
    float PosX       = -0.6f,     PosY       = 0.3f;
    float CurSize    = 8.0f;

    // GPU resources
    bgfx::ProgramHandle Program           = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParticleUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ResolutionUniform = BGFX_INVALID_HANDLE;
};
