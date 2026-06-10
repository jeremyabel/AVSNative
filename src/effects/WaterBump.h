#pragma once

#include "engine/Reflect.h"

#include <cstdint>
#include <vector>

struct WaterBumpConfig
{
    int  Fluidity      = 6;
    int  Depth         = 600;
    bool Random        = false;
    int  DropPositionX = 1;  // 0=left  1=center  2=right
    int  DropPositionY = 1;  // 0=top   1=center  2=bottom
    int  DropRadius    = 40;
};

class WaterBump : public ReflectedEffect<WaterBumpConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI(&WaterBumpConfig::Fluidity,      "fluidity",      "Fluidity",    2,   10),
            RangeI(&WaterBumpConfig::Depth,          "depth",         "Depth",       100, 2000),
            Bool  (&WaterBumpConfig::Random,         "random",        "Random Drop"),
            SelectI(&WaterBumpConfig::DropPositionX, "dropPositionX", "Drop X",
                    { "Left", "Center", "Right" }),
            SelectI(&WaterBumpConfig::DropPositionY, "dropPositionY", "Drop Y",
                    { "Top",  "Center", "Bottom" }),
            RangeI(&WaterBumpConfig::DropRadius,     "dropRadius",    "Drop Radius", 10,  100),
        };
        return f;
    }
    std::string EffectName() const override { return "Water Bump"; }

private:
    void EnsureBuffers(uint16_t W, uint16_t H);
    void SineBlob(int X, int Y, int Radius, int Height);
    void CalcWater();

    bgfx::ProgramHandle Program    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle InputUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle HeightUnif = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexelUnif  = BGFX_INVALID_HANDLE;  // u_texelSize: (1/w, 1/h, 0, 0)
    bgfx::TextureHandle HeightTex  = BGFX_INVALID_HANDLE;

    std::vector<int32_t> Bufs[2];  // ping-pong CPU height buffers (int32, row-major, row 0 = top)
    std::vector<float>   UploadBuf;
    int      Page = 0;
    uint16_t BufW = 0;
    uint16_t BufH = 0;
};
