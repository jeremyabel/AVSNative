#pragma once

#include "engine/Reflect.h"

struct NVGcontext;
struct NVGLUframebuffer;

struct BassSpinConfig
{
    bool                   EnabledLeft  = true;
    bool                   EnabledRight = true;
    std::array<uint8_t, 3> ColorLeft    = { 255, 255, 255 };
    std::array<uint8_t, 3> ColorRight   = { 255, 255, 255 };
    int                    Mode         = 1;   // 0=Outline, 1=Filled
};

class BassSpin : public ReflectedEffect<BassSpinConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Bool(&BassSpinConfig::EnabledLeft, "enabledLeft", "Enabled Left"),
            Bool(&BassSpinConfig::EnabledRight, "enabledRight", "Enabled Right"),
            Color(&BassSpinConfig::ColorLeft, "colorLeft", "Color Left"),
            Color(&BassSpinConfig::ColorRight, "colorRight", "Color Right"),
            SelectI(&BassSpinConfig::Mode, "mode", "Mode", { "Outline", "Filled" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Bass Spin"; }

private:
    void EnsureOverlay(int W, int H);
    void DestroyOverlay();

    // Animation state (matches JS constructor initializers)
    float LastA   = 0.0f;
    float Rv[2]   = { 3.14159265358979323846f, 0.0f };  // rotation accumulator per triangle
    float V[2]    = { 0.0f, 0.0f };                     // angular velocity per triangle
    float Dir[2]  = { -1.0f, 1.0f };                    // CW / CCW
    float Lx[2][2] = {};   // previous tip x [point][tri]
    float Ly[2][2] = {};   // previous tip y [point][tri]

    // NanoVG
    NVGcontext*       m_nvg        = nullptr;
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0;
    int               m_overlayH   = 0;

    // bgfx composite pass (reuses fs_simple.sc / u_simpleParams names)
    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
