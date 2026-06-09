#pragma once

#include "engine/Reflect.h"

struct ClearConfig
{
    std::array<uint8_t, 3> Color = { 0, 0, 0 };
};

class Clear : public ReflectedEffect<ClearConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Color(&ClearConfig::Color, "color", "Color"),
        };
        return f;
    }
    std::string EffectName() const override { return "Clear"; }

private:
    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
};
