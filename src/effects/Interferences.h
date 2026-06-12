#pragma once

#include "engine/Effect.h"

class Interferences : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int   NPoints      = 2;      // 0–8
    float Distance     = 10.0f;  // 1–64
    float Alpha        = 128.0f; // 1–255
    float Rotation     = 0.0f;   // 0–255; initial rotation; advanced at runtime in Render
    float RotationInc  = 0.0f;   // -32–32
    float Distance2    = 32.0f;  // 1–64
    float Alpha2       = 192.0f; // 1–255
    float RotationInc2 = 25.0f;  // -32–32
    bool  RGB          = true;
    int   OutBlend     = 0;
    bool  OnBeat       = true;
    float Speed        = 0.2f;   // 0.01–1.28
    // When true (default), the vertical sampling offset is negated so the
    // rotation direction matches the win32 original. When false it matches the
    // AVSWeb JS reference (whose rotation is mirrored from win32).
    bool  ReverseRotation = true;

    static constexpr const char* kNPoints         = "nPoints";
    static constexpr const char* kAlpha           = "alpha";
    static constexpr const char* kDistance        = "distance";
    static constexpr const char* kRotationInc     = "rotationinc";
    static constexpr const char* kAlpha2          = "alpha2";
    static constexpr const char* kDistance2       = "distance2";
    static constexpr const char* kRotationInc2    = "rotationinc2";
    static constexpr const char* kRotation        = "rotation";
    static constexpr const char* kSpeed           = "speed";
    static constexpr const char* kOnBeat          = "onbeat";
    static constexpr const char* kRGB             = "rgb";
    static constexpr const char* kReverseRotation = "reverseRotation";
    static constexpr const char* kOutBlend        = "outBlend";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Interferences"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // Runtime state
    float Status = 3.14159265358979323846f;  // beat oscillation phase; starts at π (resting)

    bgfx::ProgramHandle Program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets0      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets1      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets2      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Offsets3      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
