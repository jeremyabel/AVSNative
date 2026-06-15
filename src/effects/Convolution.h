#pragma once

#include "engine/Effect.h"

#include <array>
#include <bgfx/bgfx.h>

class Convolution : public Effect
{
public:

    Convolution() { Kernel[24] = 1; }

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Convolution Filter"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    bool EnableWrap = false;
    bool EnableAbsolute = false;
    bool EnableTwoPass = false;
    int Bias = 0;
    int Scale = 1;
    std::array<int, 49> Kernel{};

private:

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle KernelUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params1Uniform  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params2Uniform = BGFX_INVALID_HANDLE;
};
