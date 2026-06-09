#pragma once

#include "engine/Reflect.h"

#include <bgfx/bgfx.h>

#include <array>
#include <vector>

struct NVGcontext;
struct NVGLUframebuffer;

struct RotatingStarsConfig
{
    std::vector<std::array<uint8_t,3>> Colors = {{ {255, 255, 255} }};
};

class RotatingStars : public ReflectedEffect<RotatingStarsConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Colors(&RotatingStarsConfig::Colors, "colors", "Colors"),
        };
        return f;
    }
    std::string EffectName() const override { return "Rotating Stars"; }

private:
    void EnsureOverlay(int W, int H);
    void DestroyOverlay();

    NVGcontext*       m_nvg        = nullptr;
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0, m_overlayH = 0;

    float m_r        = 0.0f;   // global rotation accumulator (+0.1 per frame)
    int   m_colorPos = 0;      // color cycle position (0 .. colors.size()*64 - 1)

    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
