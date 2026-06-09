#pragma once

#include "engine/Reflect.h"

#include <bgfx/bgfx.h>

#include <array>
#include <vector>

struct NVGcontext;
struct NVGLUframebuffer;

struct RingConfig
{
    std::vector<std::array<uint8_t,3>> Colors = {{ {255, 255, 255} }};
    int Size         = 8;   // 1..64  — radius fraction: Size/32 of min(W,H)
    int AudioSource  = 0;   // 0=Waveform, 1=Spectrum
    int AudioChannel = 2;   // 0=Left, 1=Right, 2=Center
    int Position     = 2;   // 0=Left, 1=Right, 2=Center
};

class Ring : public ReflectedEffect<RingConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Colors  (&RingConfig::Colors,       "colors",       "Colors"),
            RangeI  (&RingConfig::Size,          "size",         "Size",          1, 64),
            SelectI (&RingConfig::AudioSource,   "audioSource",  "Audio Source",
                     {"Waveform", "Spectrum"}),
            SelectI (&RingConfig::AudioChannel,  "audioChannel", "Audio Channel",
                     {"Left", "Right", "Center"}),
            SelectI (&RingConfig::Position,      "position",     "Position",
                     {"Left", "Right", "Center"}),
        };
        return f;
    }
    std::string EffectName() const override { return "Ring"; }

private:
    void EnsureOverlay(int W, int H);
    void DestroyOverlay();

    NVGcontext*       m_nvg        = nullptr;
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0, m_overlayH = 0;

    int m_colorPos = 0;

    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
