#pragma once

#include "engine/Reflect.h"

struct MultiFilterConfig
{
    int  EffectMode   = 0;
    bool ToggleOnBeat = false;
};

class MultiFilter : public ReflectedEffect<MultiFilterConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&MultiFilterConfig::EffectMode, "effect", "Effect",
                    { "Chrome", "Double Chrome", "Triple Chrome", "Infroot + Border Convolution" }),
            Bool(&MultiFilterConfig::ToggleOnBeat, "toggleOnBeat", "Toggle On Beat"),
        };
        return f;
    }
    std::string EffectName() const override { return "Multi Filter"; }

private:
    // Runtime toggle state; starts active
    bool ToggleState = true;

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
