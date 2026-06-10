#pragma once

#include "engine/Reflect.h"

struct ScatterConfig
{
};

class Scatter : public ReflectedEffect<ScatterConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {};
        return f;
    }
    std::string EffectName() const override { return "Scatter"; }

private:
    uint32_t SeedFrame = 0;

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
