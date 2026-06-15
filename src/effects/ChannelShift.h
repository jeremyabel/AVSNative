#pragma once

#include "engine/Effect.h"

class ChannelShift : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    [[nodiscard]] std::string Name() const override { return "Channel Shift"; }
    [[nodiscard]] nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    int Mode = 0; // 0=RGB 1=RBG 2=GRB 3=GBR 4=BRG 5=BGR
    bool OnBeatRandom = false;

private:

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
