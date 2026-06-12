#pragma once

#include "engine/Effect.h"

class BlitEffect : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    float Zoom = 1.05f;     // 0.8–1.5
    float Rotation = 0.0f;  // -0.1–0.1
    float CenterX = 0.5f;   // 0–1
    float CenterY = 0.5f;   // 0–1
    // Matches the win32 Blitter Feedback default (bilinear off = nearest). With
    // this off, the zoom-feedback stays crisp like the original; bilinear softens
    // and blooms the buffer across frames.
    bool  Bilinear = false;
    // When bilinear is on, use the original AVS 8-bit integer 2x2 blend instead of
    // hardware bilinear (bit-exact match to win32). Ignored when Bilinear is off.
    bool  Compat   = false;

    static constexpr const char* kZoom     = "zoom";
    static constexpr const char* kRotation = "rotation";
    static constexpr const char* kCenterX  = "centerX";
    static constexpr const char* kCenterY  = "centerY";
    static constexpr const char* kBilinear = "bilinear";
    static constexpr const char* kCompat   = "bilinearCompat";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Blit"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    float Angle = 0.f;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle FlagsUniform = BGFX_INVALID_HANDLE;
};
