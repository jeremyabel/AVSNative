#pragma once

#include "engine/Reflect.h"

struct MosaicConfig
{
    int BlockSize = 8;
};

class Mosaic : public ReflectedEffect<MosaicConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI(&MosaicConfig::BlockSize, "blockSize", "Block Size", 1, 64),
        };
        return f;
    }
    std::string EffectName() const override { return "Mosaic"; }

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
