#pragma once

#include "engine/Reflect.h"

struct OnBeatClearConfig
{
    std::array<uint8_t, 3> Color = { 255, 255, 255 };
    bool                   Blend = false;
    int                    Nf    = 1;   // clear every N beats; 0 = disabled
};

class OnBeatClear : public ReflectedEffect<OnBeatClearConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Color(&OnBeatClearConfig::Color, "color", "Color"),
            Bool(&OnBeatClearConfig::Blend, "blend", "Blend"),
            RangeI(&OnBeatClearConfig::Nf, "nf", "Every N Beats", 0, 100),
        };
        return f;
    }
    std::string EffectName() const override { return "OnBeat Clear"; }

private:
    // Runtime counters
    int32_t Cf = 0;   // beats since last clear
    int32_t Df = 0;   // non-beat frames since last clear

    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
};
