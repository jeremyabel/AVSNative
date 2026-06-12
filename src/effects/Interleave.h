#pragma once

#include "engine/Effect.h"

#include <array>

class Interleave : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    float                  X        = 1.0f;  // 0–64
    float                  Y        = 1.0f;  // 0–64
    float                  X2       = 1.0f;  // 0–64
    float                  Y2       = 1.0f;  // 0–64
    int                    BeatDur  = 4;     // 1–64
    std::array<uint8_t, 3> Color    = { 0, 0, 0 };
    bool                   OnBeat   = false;
    int                    OutBlend = 0;

    static constexpr const char* kX        = "x";
    static constexpr const char* kY        = "y";
    static constexpr const char* kX2       = "x2";
    static constexpr const char* kY2       = "y2";
    static constexpr const char* kBeatDur  = "beatdur";
    static constexpr const char* kColor    = "color";
    static constexpr const char* kOnBeat   = "onbeat";
    static constexpr const char* kOutBlend = "outBlend";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Interleave"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Snaps the animated grid position to X/Y. Called after Deserialize and by
    // the UI when the X/Y sliders change.
    void ResetAnim() { CurX = X; CurY = Y; }

private:
    // Runtime state (animated positions)
    float CurX = 1.0f;
    float CurY = 1.0f;

    bgfx::ProgramHandle Program      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle GridUniform  = BGFX_INVALID_HANDLE;
};
