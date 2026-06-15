#pragma once

#include "engine/Effect.h"

#include <array>
#include <cstdint>

struct StarVertex
{
    float X, Y;
    uint8_t R, G, B, A;
};

struct Star
{
    float X, Y;
    float Z;
    float SpeedMult;
};

class Starfield : public Effect
{
public:

    static constexpr const char* NAME_Color = "color";
    static constexpr const char* NAME_BlendMode = "blendMode";
    static constexpr const char* NAME_Speed = "speed";
    static constexpr const char* NAME_StarCount = "starCount";
    static constexpr const char* NAME_EnableOnBeatChange = "onBeat";
    static constexpr const char* NAME_OnBeatSpeed = "onBeatSpeed";
    static constexpr const char* NAME_OnBeatDuration = "onBeatDuration";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Starfield"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Applies a Speed change to the live animation (unless an on-beat speed burst
    // is still cooling down). Called after Deserialize and by the UI.
    void ResetSpeed()
    {
        if (Cooldown <= 0)
            CurrentSpeed = Speed;
    }

    // Re-seeds the star array after StarCount changes. Called after Deserialize
    // and by the UI; no-op until the first Render establishes dimensions.
    void ReinitStars()
    {
        if (LastW > 0)
            InitStars(LastW, LastH);
    }

public:

    std::array<uint8_t, 3> Color = { 255, 255, 255 };
    int BlendMode = 0; // 0=Replace, 1=Additive, 2=50/50
    float Speed = 6.0f;
    int StarCount = 350;
    bool OnBeat = false;
    float OnBeatSpeed = 4.0f;
    int OnBeatDuration = 15;

private:

    void InitStars(int W, int H);
    void ResetStar(int Idx, int W, int H, int XOff, int YOff);
    static void Colorize(uint8_t Bright, uint8_t Cr, uint8_t Cg, uint8_t Cb, uint8_t& OutR, uint8_t& OutG, uint8_t& OutB);

    // Runtime state
    float CurrentSpeed = 6.0f;
    float OnBeatDiff = 0.0f;
    int32_t Cooldown = 0;

    static constexpr int32_t kMaxStars = 4096;
    std::array<Star, kMaxStars> Stars;
    int32_t AbsStars = 0;
    int32_t LastW = 0;
    int32_t LastH = 0;

    // GPU resources
    bgfx::ProgramHandle BlitProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle StarProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BlitTexUniform = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle BlitQuadVB = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout StarLayout;
};
