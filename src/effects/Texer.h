#pragma once

#include "engine/Effect.h"

#include <bgfx/bgfx.h>
#include <chrono>
#include <string>
#include <vector>

// Texer: CPU-side stamp effect. Scans the input framebuffer for non-black pixels, then
// additively stamps a user-provided image (centered on each hit pixel) into the output.
// addToInput: composite stamps on top of the input; Colorize: multiply image by pixel color.
// See ref/AVSWeb/src/effects/texer.js.
class Texer : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::string ImageData    = "";     // bundle asset ref/name; empty = built-in soft-dot
    bool        AddToInput   = false;
    bool        Colorize     = false;
    int         NumParticles = 100;   // max stamps per frame (1–1024)

    static constexpr const char* kImageData    = "imageData";
    static constexpr const char* kAddToInput   = "addToInput";
    static constexpr const char* kColorize     = "colorize";
    static constexpr const char* kNumParticles = "numParticles";

    void Init() override;
    void Destroy() override;
    void Render(const RenderContext& Context) override;

    std::string Name() const override { return "Texer"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Uses 2 views: blitViewId (blit input→staging) + drawViewId (upload→output FBO).
    uint8_t ExpectedViewCount() const override { return 2; }

    std::vector<PresetAsset> CollectAssets() const override;
    void ApplyAsset(const std::string& key, const std::string& name,
                    std::vector<uint8_t> bytes) override;

    int GetImageW() const { return m_imgW; }
    int GetImageH() const { return m_imgH; }
    // Number of animation frames (>1 for an animated GIF, 0 for a static image).
    int GetFrameCount() const { return (int)m_frames.size(); }

private:
    void BuildFromRaw(const std::vector<uint8_t>& raw);
    void MakeDefaultImage();
    void ResetAnimation();
    void AdvanceAnimation();   // advance m_curFrame by wall-clock time, refresh m_imgPixels
    void EnsureBuffers(int w, int h);
    void Stamp(int cx, int cy, uint8_t cr, uint8_t cg, uint8_t cb, int fbW, int fbH);

    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_texUnif    = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_stagingTex = BGFX_INVALID_HANDLE;  // READ_BACK+BLIT_DST
    bgfx::TextureHandle m_outTex     = BGFX_INVALID_HANDLE;  // upload texture for output

    std::vector<uint8_t> m_imgPixels;    // current frame, RGBA8 (what Stamp() reads)
    std::vector<uint8_t> m_raw;          // original image bytes (for re-bundling)
    std::string          m_name;         // original filename
    int m_imgW = 0, m_imgH = 0;

    // Animated-GIF playback. m_frames is empty for a static image (single decoded frame
    // lives only in m_imgPixels). For an animated GIF it holds every frame and m_imgPixels
    // is a copy of the currently displayed one, swapped in by AdvanceAnimation().
    std::vector<std::vector<uint8_t>> m_frames;        // all frames, RGBA8 (m_imgW*m_imgH*4 each)
    std::vector<int>                  m_frameDelaysMs; // per-frame delay (ms), parallel to m_frames
    size_t m_curFrame      = 0;
    double m_frameAccumMs  = 0.0;                      // time accumulated toward next frame
    std::chrono::steady_clock::time_point m_lastTick{};
    bool   m_haveTick      = false;

    std::vector<uint8_t> m_readBuf;      // CPU readback of input (1-frame lag)
    std::vector<uint8_t> m_outBuf;       // stamped output, uploaded each frame
    int m_bufW = 0, m_bufH = 0;

    bool m_hasReadback = false;  // true once first blit+readTexture has been scheduled
    bool m_inited      = false;
};
