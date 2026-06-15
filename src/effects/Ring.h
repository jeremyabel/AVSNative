#pragma once

#include "engine/Effect.h"
#include "engine/ColorList.h"

#include <bgfx/bgfx.h>

struct NVGcontext;
struct NVGLUframebuffer;

class Ring : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    ColorList Colors;
    int Size         = 8;   // 1..64  — radius fraction: Size/32 of min(W,H)
    int AudioSource  = 0;   // 0=Waveform, 1=Spectrum
    int AudioChannel = 2;   // 0=Left, 1=Right, 2=Center
    int Position     = 2;   // 0=Left, 1=Right, 2=Center

    static constexpr const char* kColors       = "colors";
    static constexpr const char* kSize         = "size";
    static constexpr const char* kAudioSource  = "audioSource";
    static constexpr const char* kAudioChannel = "audioChannel";
    static constexpr const char* kPosition     = "position";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Ring"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    void EnsureOverlay(int W, int H);
    void DestroyOverlay();

    NVGcontext*       m_nvg        = nullptr;
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0, m_overlayH = 0;

    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
