#include "RotatingStars.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <nanovg/nanovg.h>
#include <nanovg/nanovg_bgfx.h>

#include <algorithm>
#include <cmath>

static constexpr float k4PiOver5 = 3.14159265358979323846f * 4.0f / 5.0f;

// ── Init / Destroy ────────────────────────────────────────────────────────────

void RotatingStars::Init()
{
    const bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_simple_spv,    sizeof(fs_simple_spv)));
    m_program = bgfx::createProgram(VS, FS, true);

    m_inputSampler   = bgfx::createUniform("s_input",       bgfx::UniformType::Sampler);
    m_overlaySampler = bgfx::createUniform("s_overlay",     bgfx::UniformType::Sampler);
    m_paramsUniform  = bgfx::createUniform("u_simpleParams", bgfx::UniformType::Vec4);

    m_nvg = nvgCreate(1, 0);  // edge AA on for smooth 1px lines
}

void RotatingStars::EnsureOverlay(int W, int H)
{
    if (m_overlayW == W && m_overlayH == H) return;
    DestroyOverlay();
    m_overlayFbo = nvgluCreateFramebuffer(m_nvg, W, H, 0);
    m_overlayW   = W;
    m_overlayH   = H;
}

void RotatingStars::DestroyOverlay()
{
    if (m_overlayFbo)
    {
        nvgluDeleteFramebuffer(m_overlayFbo);
        m_overlayFbo = nullptr;
    }
    m_overlayW = m_overlayH = 0;
}

void RotatingStars::Destroy()
{
    DestroyOverlay();

    if (m_nvg)
    {
        nvgDelete(m_nvg);
        m_nvg = nullptr;
    }

    if (bgfx::isValid(m_paramsUniform))  bgfx::destroy(m_paramsUniform);
    if (bgfx::isValid(m_overlaySampler)) bgfx::destroy(m_overlaySampler);
    if (bgfx::isValid(m_inputSampler))   bgfx::destroy(m_inputSampler);
    if (bgfx::isValid(m_program))        bgfx::destroy(m_program);

    m_paramsUniform  = BGFX_INVALID_HANDLE;
    m_overlaySampler = BGFX_INVALID_HANDLE;
    m_inputSampler   = BGFX_INVALID_HANDLE;
    m_program        = BGFX_INVALID_HANDLE;
}

// ── Render ────────────────────────────────────────────────────────────────────

void RotatingStars::Render(const RenderContext& Context)
{
    if (Cfg.Colors.empty()) return;

    const int W = Context.Width;
    const int H = Context.Height;

    EnsureOverlay(W, H);
    if (!m_overlayFbo) return;

    // Color cycling: linear interpolation between adjacent color entries.
    // Each segment spans 64 frames; frac ∈ [0,63].
    // Integer truncation matches the JS reference (Math.trunc on integer arithmetic).
    const int n = (int)Cfg.Colors.size();
    m_colorPos = (m_colorPos + 1) % (n * 64);
    const int frac = m_colorPos & 63;
    const int seg  = m_colorPos / 64;
    const auto& c1 = Cfg.Colors[seg % n];
    const auto& c2 = Cfg.Colors[(seg + 1) % n];
    const float cr = (float)((c1[0] * (63 - frac) + c2[0] * frac) / 64) / 255.0f;
    const float cg = (float)((c1[1] * (63 - frac) + c2[1] * frac) / 64) / 255.0f;
    const float cb = (float)((c1[2] * (63 - frac) + c2[2] * frac) / 64) / 255.0f;
    const NVGcolor col = nvgRGBf(cr, cg, cb);

    // ── NanoVG overlay pass ───────────────────────────────────────────────────
    nvgluSetViewFramebuffer(Context.ViewId, m_overlayFbo);
    bgfx::setViewClear(Context.ViewId, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)W, (uint16_t)H);

    nvgluBindFramebuffer(m_overlayFbo);
    nvgBeginFrame(m_nvg, (float)W, (float)H, 1.0f);

    // Orbit: both stars rotate around the center, on opposite sides.
    const int orbitX = (int)(std::cos(m_r) * W / 4);
    const int orbitY = (int)(std::sin(m_r) * H / 4);

    nvgStrokeColor(m_nvg, col);
    nvgStrokeWidth(m_nvg, 1.0f);

    // Draw two 5-pointed stars — one per stereo channel.
    for (int c = 0; c < 2; c++)
    {
        // Find the loudest local-maximum peak in spectrum bins 3..13.
        // A bin qualifies only if it exceeds both neighbours by at least 4.
        float s = 0.0f;
        if (Context.AudioData)
        {
            const float* spec = Context.AudioData->spec[c];
            for (int l = 3; l < 14; l++)
            {
                const float val = spec[l];
                if (val > s && val > spec[l + 1] + 4.0f && val > spec[l - 1] + 4.0f)
                    s = val;
            }
        }

        // c=0: upper-right quadrant; c=1: lower-left (opposite side).
        const float cx = (float)(W / 2 + (c == 0 ? orbitX : -orbitX));
        const float cy = (float)(H / 2 + (c == 0 ? orbitY : -orbitY));

        // Star half-size scales with peak amplitude; (s+9)/88 ≈ 0.10 at silence.
        const float vw = (W / 8.0f) * (s + 9.0f) / 88.0f;
        const float vh = (H / 8.0f) * (s + 9.0f) / 88.0f;

        // Build the 5-pointed star by connecting vertices 144° apart (4π/5 step).
        // Vertex coordinates are integer-truncated to match the JS drawLine reference.
        float angle = -m_r;
        float lx = std::trunc(std::cos(angle) * vw) + cx;
        float ly = std::trunc(std::sin(angle) * vh) + cy;
        angle += k4PiOver5;

        for (int t = 0; t < 5; t++)
        {
            const float nx = std::trunc(std::cos(angle) * vw) + cx;
            const float ny = std::trunc(std::sin(angle) * vh) + cy;
            angle += k4PiOver5;

            // Only draw if at least one endpoint is on screen (matches JS skip logic).
            const bool lOn = (lx >= 0 && lx < W && ly >= 0 && ly < H);
            const bool nOn = (nx >= 0 && nx < W && ny >= 0 && ny < H);
            if (lOn || nOn)
            {
                nvgBeginPath(m_nvg);
                nvgMoveTo(m_nvg, lx, ly);
                nvgLineTo(m_nvg, nx, ny);
                nvgStroke(m_nvg);
            }

            lx = nx;
            ly = ny;
        }
    }

    m_r += 0.1f;

    nvgEndFrame(m_nvg);
    nvgluBindFramebuffer(nullptr);

    // ── Composite pass: overlay → input (Replace where alpha > 0) ────────────
    const uint8_t compView = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(compView, Context.OutputFBO);
    bgfx::setViewRect(compView, 0, 0, (uint16_t)W, (uint16_t)H);
    bgfx::setViewClear(compView, BGFX_CLEAR_NONE);

    const float params[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // Replace mode
    bgfx::setUniform(m_paramsUniform, params);
    bgfx::setTexture(0, m_inputSampler,   Context.InputTexture);
    bgfx::setTexture(1, m_overlaySampler, bgfx::getTexture(m_overlayFbo->handle));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(compView, m_program);

    Context.FboManager->Swap();
}
