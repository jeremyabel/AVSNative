#pragma once

#include "engine/Effect.h"

class ChannelShift : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  Mode         = 0;     // 0=RGB 1=RBG 2=GRB 3=GBR 4=BRG 5=BGR
    bool OnBeatRandom = false;

    static constexpr const char* kMode         = "mode";
    static constexpr const char* kOnBeatRandom = "onBeatRandom";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Channel Shift"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
