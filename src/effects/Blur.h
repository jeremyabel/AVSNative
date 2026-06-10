#pragma once

#include "engine/Reflect.h"

struct BlurConfig
{
    int Intensity = 1; // index into {Light, Medium, Heavy}
};

class Blur : public ReflectedEffect<BlurConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&BlurConfig::Intensity, "intensity", "Intensity",
                    { "Light", "Medium", "Heavy" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Blur"; }

private:
    void EnsureScratch(uint16_t Width, uint16_t Height);
    void DestroyScratch();

    bgfx::FrameBufferHandle ScratchFBO = BGFX_INVALID_HANDLE;
    uint16_t ScratchW = 0;
    uint16_t ScratchH = 0;

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
