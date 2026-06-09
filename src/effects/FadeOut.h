#pragma once

#include "engine/Reflect.h"

struct FadeOutConfig
{
    float                  Speed = 1.0f;
    std::array<uint8_t, 3> Color = { 0, 0, 0 };
};

class FadeOut : public ReflectedEffect<FadeOutConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Range(&FadeOutConfig::Speed, "speed", "Speed", 0.0f, 1.0f, 0.01f),
            Color(&FadeOutConfig::Color, "color", "Color"),
        };
        return f;
    }
    std::string EffectName() const override { return "FadeOut"; }

private:
    bgfx::ProgramHandle Program           = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle FadeParamsUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform        = BGFX_INVALID_HANDLE;
};
