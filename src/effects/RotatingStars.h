#pragma once

#include "engine/Effect.h"
#include "engine/ColorList.h"

#include <bgfx/bgfx.h>

struct NVGcontext;
struct NVGLUframebuffer;

class RotatingStars : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    ColorList Colors;

    static constexpr const char* kColors = "colors";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Rotating Stars"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    void EnsureOverlay(int W, int H);
    void DestroyOverlay();

    NVGcontext*       m_nvg        = nullptr;
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0, m_overlayH = 0;

    float m_r        = 0.0f;   // global rotation accumulator (+0.1 per frame)

    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
