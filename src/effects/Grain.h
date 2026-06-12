#pragma once

#include "engine/Effect.h"

class Grain : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  Amount    = 100;  // 0–100
    int  BlendMode = 0;    // 0=Replace 1=Additive 2=50/50
    bool IsStatic  = false;

    static constexpr const char* kAmount    = "amount";
    static constexpr const char* kBlendMode = "blendMode";
    static constexpr const char* kIsStatic  = "isStatic";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Grain"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
