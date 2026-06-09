#pragma once

#include "engine/Reflect.h"

struct ColorReductionConfig
{
    int Levels = 7; // 1..8 bits per channel; shader receives 2^Levels
};

class ColorReduction : public ReflectedEffect<ColorReductionConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI(&ColorReductionConfig::Levels, "levels", "Levels (bits)", 1, 8),
        };
        return f;
    }
    std::string EffectName() const override { return "Color Reduction"; }

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
