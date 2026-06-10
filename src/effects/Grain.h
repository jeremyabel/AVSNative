#pragma once

#include "engine/Reflect.h"

struct GrainConfig
{
    int  Amount    = 100;  // 0–100
    int  BlendMode = 0;    // 0=Replace 1=Additive 2=50/50
    bool IsStatic  = false;
};

class Grain : public ReflectedEffect<GrainConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI(&GrainConfig::Amount, "amount", "Amount", 0, 100),
            SelectI(&GrainConfig::BlendMode, "blendMode", "Blend Mode",
                    { "Replace", "Additive", "50/50" }),
            Bool(&GrainConfig::IsStatic, "isStatic", "Static"),
        };
        return f;
    }
    std::string EffectName() const override { return "Grain"; }

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
