#pragma once

#include "engine/Reflect.h"

struct AddBordersConfig
{
    std::array<uint8_t, 3> Color = { 0, 0, 0 };
    int                    Size  = 1;   // 1–50, percentage of each dimension
};

class AddBorders : public ReflectedEffect<AddBordersConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Color(&AddBordersConfig::Color, "color", "Color"),
            RangeI(&AddBordersConfig::Size, "size", "Size", 1, 50),
        };
        return f;
    }
    std::string EffectName() const override { return "Add Borders"; }

private:
    bgfx::ProgramHandle Program             = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform          = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BorderParamsUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BorderColorUniform  = BGFX_INVALID_HANDLE;
};
