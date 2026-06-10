#pragma once

#include "engine/Reflect.h"

#include <array>
#include <vector>

struct NVGcontext;
struct NVGLUframebuffer;

struct SimpleConfig
{
    int  Mode                = 2;     // 0=solid analyzer, 1=line analyzer, 2=line scope, 3=solid scope
    int  Channel             = 0;     // 0=L, 1=R, 2=mix
    int  Position            = 1;     // 0=top, 1=center, 2=bottom
    bool AntialiasingEnabled = false;
    std::vector<std::array<uint8_t, 3>> Colors = { { 255, 255, 255 } };
};

class Simple : public ReflectedEffect<SimpleConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&SimpleConfig::Mode, "mode", "Mode",
                    { "Solid Analyzer", "Line Analyzer", "Line Scope", "Solid Scope" }),
            SelectI(&SimpleConfig::Channel, "channel", "Channel",
                    { "Left", "Right", "Mono Mix" }),
            SelectI(&SimpleConfig::Position, "position", "Position",
                    { "Top", "Center", "Bottom" }),
            Colors(&SimpleConfig::Colors, "colors", "Color"),
            Bool(&SimpleConfig::AntialiasingEnabled, "antialiasing", "Antialiasing"),
        };
        return f;
    }
    std::string EffectName() const override { return "Simple"; }

private:
    void EnsureOverlay(int Width, int Height);
    void EnsureNvgContext();
    void DestroyOverlay();
    std::array<float, 3> GetCurrentColor();

    int m_colorPos = 0;

    // NanoVG
    NVGcontext*       m_nvg        = nullptr;
    bool              m_nvgEdgeAa  = false; // tracks edgeaa used to create m_nvg
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0;
    int               m_overlayH   = 0;

    // bgfx composite pass
    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
