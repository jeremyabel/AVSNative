#pragma once

#include "engine/Reflect.h"

#include <bgfx/bgfx.h>
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

protected:
    const std::vector<Field>& Fields() const override;
    std::string EffectName() const override { return "Texer"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void LoadImage(const std::string& dataUrl);
    void MakeDefaultImage();
    void EnsureBuffers(int w, int h);
    void Stamp(int cx, int cy, uint8_t cr, uint8_t cg, uint8_t cb, int fbW, int fbH);

    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_texUnif    = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_stagingTex = BGFX_INVALID_HANDLE;  // READ_BACK+BLIT_DST
    bgfx::TextureHandle m_outTex     = BGFX_INVALID_HANDLE;  // upload texture for output

    std::vector<uint8_t> m_imgPixels;    // decoded image, RGBA8
    int m_imgW = 0, m_imgH = 0;

    std::vector<uint8_t> m_readBuf;      // CPU readback of input (1-frame lag)
    std::vector<uint8_t> m_outBuf;       // stamped output, uploaded each frame
    int m_bufW = 0, m_bufH = 0;

    bool m_hasReadback = false;  // true once first blit+readTexture has been scheduled
    bool m_inited      = false;
};
