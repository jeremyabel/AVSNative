#pragma once

#include "engine/Effect.h"

#include <array>

class UniqueTone : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::array<uint8_t, 3> Color    = { 255, 255, 255 };
    bool                   Invert   = false;
    int                    OutBlend = 0;   // 0=Replace 1=Additive 2=Average

    static constexpr const char* kColor    = "color";
    static constexpr const char* kInvert   = "invert";
    static constexpr const char* kOutBlend = "outBlend";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Unique Tone"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
