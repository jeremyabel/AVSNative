#pragma once

#include "engine/Effect.h"

#include <array>

class MovingParticle : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    // Defaults match original AVS
    std::array<uint8_t, 3> Color           = { 255, 255, 255 };
    int  Distance         = 16;   // 1..32
    int  Size             = 8;    // 1..128
    bool OnBeatSizeChange = false;
    int  OnBeatSize       = 8;    // 1..128
    int  BlendMode        = 1;    // 0=Replace 1=Additive 2=50/50 3=Default

    static constexpr const char* kColor            = "color";
    static constexpr const char* kDistance         = "distance";
    static constexpr const char* kSize             = "size";
    static constexpr const char* kOnBeatSizeChange = "onBeatSizeChange";
    static constexpr const char* kOnBeatSize       = "onBeatSize";
    static constexpr const char* kBlendMode        = "blendMode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "MovingParticle"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // Physics state — exact initial values from AVS_Remake
    float AttractorX = 0.0f,      AttractorY = 0.0f;
    float VelX       = -0.01551f, VelY       = 0.0f;
    float PosX       = -0.6f,     PosY       = 0.3f;
    float CurSize    = 8.0f;

    // GPU resources
    bgfx::ProgramHandle Program           = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParticleUniform   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ColorUniform      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ResolutionUniform = BGFX_INVALID_HANDLE;
};
