#pragma once

#include "engine/Reflect.h"

#include <bgfx/bgfx.h>

#include <array>
#include <vector>

struct NVGcontext;
struct NVGLUframebuffer;

struct OscilloscopeStarConfig
{
    std::vector<std::array<uint8_t,3>> Colors = {{ {255, 255, 255} }};
    int AudioChannel = 2;   // 0=Left, 1=Right, 2=Center
    int Position     = 2;   // 0=Left, 1=Right, 2=Center
    int Size         = 8;   // 0..32  — fraction of screen: Size/32
    int Rotation     = 0;   // -16..16 (speed): +0.01*Rotation rad/frame
};

class OscilloscopeStar : public ReflectedEffect<OscilloscopeStarConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Colors  (&OscilloscopeStarConfig::Colors,       "colors",       "Colors"),
            SelectI (&OscilloscopeStarConfig::AudioChannel, "audioChannel", "Audio Channel",
                     {"Left", "Right", "Center"}),
            SelectI (&OscilloscopeStarConfig::Position,     "position",     "Position",
                     {"Left", "Right", "Center"}),
            RangeI  (&OscilloscopeStarConfig::Size,         "size",         "Size",         0, 32),
            RangeI  (&OscilloscopeStarConfig::Rotation,     "rotation",     "Rotation Speed", -16, 16),
        };
        return f;
    }
    std::string EffectName() const override { return "Oscilloscope Star"; }

private:
    void EnsureOverlay(int W, int H);
    void DestroyOverlay();

    NVGcontext*       m_nvg        = nullptr;
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0, m_overlayH = 0;

    float m_currentRotation = 0.0f;
    int   m_colorPos        = 0;

    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
