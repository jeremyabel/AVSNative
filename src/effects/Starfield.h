#pragma once

#include "engine/Reflect.h"

#include <array>
#include <cstdint>

struct StarVertex
{
    float   X, Y;
    uint8_t R, G, B, A;
};

struct Star
{
    float X, Y;       // center-relative position (sub-pixel)
    float Z;          // depth: 255=far/new, approaches 0 as star zooms in
    float SpeedMult;  // per-star speed factor [0.1, 1.0]
};

struct StarfieldConfig
{
    // Defaults match original AVS
    std::array<uint8_t, 3> Color = { 255, 255, 255 };
    int   BlendMode      = 0;    // 0=Replace, 1=Additive, 2=50/50
    float Speed          = 6.0f;
    int   StarCount      = 350;
    bool  OnBeat         = false;
    float OnBeatSpeed    = 4.0f;
    int   OnBeatDuration = 15;
};

class Starfield : public ReflectedEffect<StarfieldConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Color(&StarfieldConfig::Color, "color", "Color"),
            SelectI(&StarfieldConfig::BlendMode, "blendMode", "Blend Mode",
                    { "Replace", "Additive", "50/50" }),
            Range(&StarfieldConfig::Speed, "speed", "Warp Speed", 1.0f, 500.0f, 1.0f),
            RangeI(&StarfieldConfig::StarCount, "starCount", "Stars", 100, 4095),
            Bool(&StarfieldConfig::OnBeat, "onBeat", "On Beat"),
            Range(&StarfieldConfig::OnBeatSpeed, "onBeatSpeed", "On-Beat Speed", 1.0f, 500.0f, 1.0f),
            RangeI(&StarfieldConfig::OnBeatDuration, "onBeatDuration", "On-Beat Duration", 1, 100),
        };
        return f;
    }
    std::string EffectName() const override { return "Starfield"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override
    {
        for (const std::string& k : changed)
        {
            if (k == "speed" && Cooldown <= 0)
                CurrentSpeed = Cfg.Speed;
            if (k == "starCount" && LastW > 0)
                InitStars(LastW, LastH);
        }
    }

private:
    void InitStars(int W, int H);
    void ResetStar(int Idx, int W, int H, int XOff, int YOff);
    static void Colorize(uint8_t Bright, uint8_t Cr, uint8_t Cg, uint8_t Cb,
                         uint8_t& OutR, uint8_t& OutG, uint8_t& OutB);

    // Runtime state
    float   CurrentSpeed = 6.0f;
    float   OnBeatDiff   = 0.0f;
    int32_t Cooldown     = 0;

    static constexpr int32_t kMaxStars = 4096;
    std::array<Star, kMaxStars> Stars;
    int32_t AbsStars = 0;
    int32_t LastW = 0, LastH = 0;

    // GPU resources
    bgfx::ProgramHandle      BlitProgram    = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      StarProgram    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle      BlitTexUniform = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle BlitQuadVB     = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout       StarLayout;
};
