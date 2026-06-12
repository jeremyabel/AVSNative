#pragma once

#include "engine/Effect.h"
#include "engine/LuaRuntime.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class SuperScope : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::string InitCode;
    std::string FrameCode;
    std::string BeatCode;
    std::string PointCode;
    std::vector<std::array<uint8_t, 3>> Colors = { { 255, 255, 255 } };
    int AudioSource  = 0;         // 0 = waveform, 1 = spectrum
    int AudioChannel = 0;         // 0 = center, 1 = left, 2 = right
    int DrawMode     = 1;         // 0 = dots, 1 = lines

    static constexpr const char* kInitCode     = "initCode";
    static constexpr const char* kFrameCode    = "frameCode";
    static constexpr const char* kBeatCode     = "beatCode";
    static constexpr const char* kPointCode    = "pointCode";
    static constexpr const char* kColors       = "colors";
    static constexpr const char* kAudioSource  = "audioSource";
    static constexpr const char* kAudioChannel = "audioChannel";
    static constexpr const char* kDrawMode     = "drawMode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Super Scope"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetScriptError(const std::string& paramName) const override;

    // Recompiles all four Lua blocks, reseeds user vars, reruns init, and rebuilds
    // the point loop. Called after Deserialize and by the UI when any code editor
    // changes.
    void Recompile();

private:
    // ── Script state ─────────────────────────────────────────────────────────
    LuaRuntime m_lua;

    int m_initRef  = -1;
    int m_frameRef = -1;
    int m_beatRef  = -1;
    int m_pointRef = -1;   // ref to the compiled point-loop function

    bool m_inited = false;

    // ── Color cycling (mirrors JS color_pos logic) ────────────────────────────
    int m_colorPos = 0;
    struct Rgb { float r, g, b; };
    Rgb AdvanceColor();

    // ── CPU overlay buffer ────────────────────────────────────────────────────
    int                  m_overlayW = 0, m_overlayH = 0;
    std::vector<uint8_t> m_overlayBuf;  // RGBA8, row-major, top-down
    static constexpr int kOutStride = 7; // x,y,r,g,b,skip,drawmode per point
    std::vector<float>   m_outBuf;      // kAudioBins * kOutStride

    // Audio sample array built per-frame from VisData.
    std::vector<float>   m_audioBuf;    // kAudioBins values in [0, 255]

    void EnsureOverlay(int w, int h);
    void BuildAudioBuf(const RenderContext& ctx);
    void SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    void DrawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);

    void SeedUserVars();
    void RebuildPointLoop();

    // ── bgfx resources ────────────────────────────────────────────────────────
    bgfx::ProgramHandle    m_program       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle    m_uBase         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle    m_uOverlay      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle    m_uParams       = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle    m_overlayTex    = BGFX_INVALID_HANDLE;
};
