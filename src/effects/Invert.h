#pragma once

#include "engine/Reflect.h"

struct InvertConfig
{
};

class Invert : public ReflectedEffect<InvertConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {};
        return f;
    }
    std::string EffectName() const override { return "Invert"; }

private:
    bgfx::ProgramHandle Program    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
};
