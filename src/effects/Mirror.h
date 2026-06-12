#pragma once

#include "engine/Effect.h"

class Mirror : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    bool FlipX = true;
    bool FlipY = false;
    bool OnBeat = false;

    static constexpr const char* kFlipX  = "flipX";
    static constexpr const char* kFlipY  = "flipY";
    static constexpr const char* kOnBeat = "onBeat";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Mirror"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bool IsBeatActive = false; // runtime toggle state, not serialized

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
