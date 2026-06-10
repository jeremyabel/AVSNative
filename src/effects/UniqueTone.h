#pragma once

#include "engine/Reflect.h"

struct UniqueToneConfig
{
    std::array<uint8_t, 3> Color    = { 255, 255, 255 };
    bool                   Invert   = false;
    int                    OutBlend = 0;   // 0=Replace 1=Additive 2=Average
};

class UniqueTone : public ReflectedEffect<UniqueToneConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Color(&UniqueToneConfig::Color, "color", "Color"),
            Bool(&UniqueToneConfig::Invert, "invert", "Invert"),
            SelectI(&UniqueToneConfig::OutBlend, "outBlend", "Blend",
                    { "Replace", "Additive", "Average" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Unique Tone"; }

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
