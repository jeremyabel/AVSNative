#pragma once

#include "engine/Effect.h"

#include <array>
#include <vector>

struct NVGcontext;
struct NVGLUframebuffer;

class Simple : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  Mode                = 2;     // 0=solid analyzer, 1=line analyzer, 2=line scope, 3=solid scope
    int  Channel             = 0;     // 0=L, 1=R, 2=mix
    int  Position            = 1;     // 0=top, 1=center, 2=bottom
    bool AntialiasingEnabled = false;
    std::vector<std::array<uint8_t, 3>> Colors = { { 255, 255, 255 } };

    static constexpr const char* kMode         = "mode";
    static constexpr const char* kChannel      = "channel";
    static constexpr const char* kPosition     = "position";
    static constexpr const char* kColors       = "colors";
    static constexpr const char* kAntialiasing = "antialiasing";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Simple"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

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
