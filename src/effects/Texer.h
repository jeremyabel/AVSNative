#pragma once

#include "engine/Reflect.h"

#include <bgfx/bgfx.h>
#include <chrono>
#include <string>
#include <vector>

// Texer: CPU-side stamp effect. Scans the input framebuffer for non-black pixels, then
// additively stamps a user-provided image (centered on each hit pixel) into the output.
// addToInput: composite stamps on top of the input; Colorize: multiply image by pixel color.
// See ref/AVSWeb/src/effects/texer.js.
struct TexerConfig
{
    std::string ImageData    = "";     // base64 data URL; empty = built-in soft-dot
    bool        AddToInput   = false;
    bool        Colorize     = false;
    int         NumParticles = 100;   // max stamps per frame
};

class Texer : public ReflectedEffect<TexerConfig>
{
public:
    void Init() override;
    void Destroy()                               override;
    void Render(const RenderContext& Context)    override;

    // Uses 2 views: blitViewId (blit input→staging) + drawViewId (upload→output FBO).
    uint8_t ExpectedViewCount() const override { return 2; }

    // ImageData is too large for the reflection table; handled via GetConfig/SetConfig.
    nlohmann::json GetConfig() const override;
    void           SetConfig(const nlohmann::json& cfg) override;

    int GetImageW() const { return m_imgW; }
    int GetImageH() const { return m_imgH; }
    // Number of animation frames (>1 for an animated GIF, 0 for a static image).
    int GetFrameCount() const { return (int)m_frames.size(); }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> kFields = {
            ::Bool(&TexerConfig::AddToInput,   "addToInput",   "Add to Input"),
            ::Bool(&TexerConfig::Colorize,     "colorize",     "Colorize"),
            RangeI(&TexerConfig::NumParticles, "numParticles", "Particles", 1, 1024),
        };
        return kFields;
    }
    std::string EffectName() const override { return "Texer"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void LoadImage(const std::string& dataUrl);
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
