#pragma once

#include "engine/Reflect.h"

struct MultiplierConfig
{
    int Mode = 3;  // default: ×2
};

class Multiplier : public ReflectedEffect<MultiplierConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&MultiplierConfig::Mode, "mode", "Mode",
                    { "Inv (non-black to white)", "x8", "x4", "x2",
                      "x1/2", "x1/4", "x1/8", "XS (white only)" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Multiplier"; }

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
