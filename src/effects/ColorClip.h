#pragma once

#include "engine/Reflect.h"

struct ColorClipConfig
{
    std::array<uint8_t, 3> Color = { 32, 32, 32 };
};

class ColorClip : public ReflectedEffect<ColorClipConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Color(&ColorClipConfig::Color, "color", "Clip Color"),
        };
        return f;
    }
    std::string EffectName() const override { return "Color Clip"; }

private:
    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
};
