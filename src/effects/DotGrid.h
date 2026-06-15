#pragma once

#include "engine/Effect.h"
#include "engine/ColorList.h"

class DotGrid : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Dot Grid"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    void ResetColorCycle() { Colors.ResetCycle(); }

public:

    ColorList Colors;
    int Spacing = 8;
    int SpeedX = 128;
    int SpeedY = 128;
    int BlendMode = 3;

private:

    int32_t Xp = 0;
    int32_t Yp = 0;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle GridUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle SizeUniform = BGFX_INVALID_HANDLE;
};
