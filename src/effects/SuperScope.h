#pragma once

#include "engine/Reflect.h"
#include "engine/LuaRuntime.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct SuperScopeConfig
{
    std::string InitCode;
    std::string FrameCode;
    std::string BeatCode;
    std::string PointCode;
    std::vector<std::array<uint8_t, 3>> Colors = { { 255, 255, 255 } };
    int AudioSource  = 0;         // 0 = waveform, 1 = spectrum
    int AudioChannel = 0;         // 0 = center, 1 = left, 2 = right
    int DrawMode     = 1;         // 0 = dots, 1 = lines
};

class SuperScope : public ReflectedEffect<SuperScopeConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;
    std::string GetScriptError(const std::string& paramName) const override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Lua(&SuperScopeConfig::InitCode, "initCode", "Init"),
            Lua(&SuperScopeConfig::FrameCode, "frameCode", "Frame"),
            Lua(&SuperScopeConfig::BeatCode, "beatCode", "Beat"),
            Lua(&SuperScopeConfig::PointCode, "pointCode", "Point"),
            Colors(&SuperScopeConfig::Colors, "colors", "Color"),
            SelectI(&SuperScopeConfig::AudioSource, "audioSource", "Source", { "Waveform", "Spectrum" }),
            SelectI(&SuperScopeConfig::AudioChannel, "audioChannel", "Channel", { "Center", "Left", "Right" }),
            SelectI(&SuperScopeConfig::DrawMode, "drawMode", "Draw", { "Dots", "Lines" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Super Scope"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override
    {
        bool codeChanged = false;
        for (const std::string& k : changed)
            if (k == "initCode" || k == "frameCode" || k == "beatCode" || k == "pointCode")
            {
                codeChanged = true;
                break;
            }
        if (!codeChanged)
            return;

        m_lua.CompileBlock(Cfg.InitCode,  "initCode",  m_initRef);
        m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
        m_lua.CompileBlock(Cfg.BeatCode,  "beatCode",  m_beatRef);
        SeedUserVars();
        m_lua.RunBlock(m_initRef, "initCode");
        m_inited = true;
        RebuildPointLoop();
    }

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
