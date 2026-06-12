#pragma once

#include "engine/Effect.h"

class MultiFilter : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  EffectMode   = 0;
    bool ToggleOnBeat = false;

    static constexpr const char* kEffectMode   = "effect";
    static constexpr const char* kToggleOnBeat = "toggleOnBeat";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Multi Filter"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // Runtime toggle state; starts active
    bool ToggleState = true;

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
