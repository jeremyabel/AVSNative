#pragma once

#include "engine/Effect.h"

#include <bgfx/bgfx.h>

#include <array>
#include <vector>

struct NVGcontext;
struct NVGLUframebuffer;

class OscilloscopeStar : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::vector<std::array<uint8_t,3>> Colors = {{ {255, 255, 255} }};
    int AudioChannel = 2;   // 0=Left, 1=Right, 2=Center
    int Position     = 2;   // 0=Left, 1=Right, 2=Center
    int Size         = 8;   // 0..32  — fraction of screen: Size/32
    int Rotation     = 0;   // -16..16 (speed): +0.01*Rotation rad/frame

    static constexpr const char* kColors       = "colors";
    static constexpr const char* kAudioChannel = "audioChannel";
    static constexpr const char* kPosition     = "position";
    static constexpr const char* kSize         = "size";
    static constexpr const char* kRotation     = "rotation";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Oscilloscope Star"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

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
