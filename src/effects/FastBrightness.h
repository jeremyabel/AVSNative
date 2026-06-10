#pragma once

#include "engine/Reflect.h"

struct FastBrightnessConfig
{
    int Dir = 0; // 0=×2 brighter, 1=×½ darker, 2=no change
};

class FastBrightness : public ReflectedEffect<FastBrightnessConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&FastBrightnessConfig::Dir, "dir", "Mode",
                    { "×2 Brighter", "×½ Darker", "No Change" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Fast Brightness"; }

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
