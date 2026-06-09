#include "Ring.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <nanovg/nanovg.h>
#include <nanovg/nanovg_bgfx.h>

#include <algorithm>
#include <cmath>

static constexpr float kTwoPiOver80 = 3.14159265358979323846f * 2.0f / 80.0f;

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Ring::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    const bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_simple_spv,    sizeof(fs_simple_spv)));
    m_program = bgfx::createProgram(VS, FS, true);

    m_inputSampler   = bgfx::createUniform("s_input",        bgfx::UniformType::Sampler);
    m_overlaySampler = bgfx::createUniform("s_overlay",      bgfx::UniformType::Sampler);
    m_paramsUniform  = bgfx::createUniform("u_simpleParams",  bgfx::UniformType::Vec4);

    m_nvg = nvgCreate(1, 0);
}

void Ring::EnsureOverlay(int W, int H)
{
    if (m_overlayW == W && m_overlayH == H) return;
    DestroyOverlay();
    m_overlayFbo = nvgluCreateFramebuffer(m_nvg, W, H, 0);
    m_overlayW   = W;
    m_overlayH   = H;
}

void Ring::DestroyOverlay()
{
    if (m_overlayFbo) { nvgluDeleteFramebuffer(m_overlayFbo); m_overlayFbo = nullptr; }
    m_overlayW = m_overlayH = 0;
}

void Ring::Destroy()
{
    DestroyOverlay();
    if (m_nvg)               { nvgDelete(m_nvg); m_nvg = nullptr; }
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

void Ring::Render(const RenderContext& Context)
{
    if (Cfg.Colors.empty()) return;

    const int W = Context.Width;
    const int H = Context.Height;

    EnsureOverlay(W, H);
    if (!m_overlayFbo) return;

    // Color cycling
    const int n = (int)Cfg.Colors.size();
    m_colorPos = (m_colorPos + 1) % (n * 64);
    const int frac = m_colorPos & 63;
    const int seg  = m_colorPos / 64;
    const auto& c1 = Cfg.Colors[seg % n];
    const auto& c2 = Cfg.Colors[(seg + 1) % n];
    const float cr = (float)((c1[0] * (63 - frac) + c2[0] * frac) / 64) / 255.0f;
    const float cg = (float)((c1[1] * (63 - frac) + c2[1] * frac) / 64) / 255.0f;
    const float cb = (float)((c1[2] * (63 - frac) + c2[2] * frac) / 64) / 255.0f;

    // Audio data preparation — waveform or spectrum, left/right/center
    float faData[kAudioBins] = {};
    if (Context.AudioData)
    {
        const VisData& vis = *Context.AudioData;
        if (Cfg.AudioChannel == 2)
        {
            if (Cfg.AudioSource == 0)
            {
                // center waveform: signed int8 average → uint8 encoding
                for (int i = 0; i < kAudioBins; i++)
                {
                    const int v0 = std::clamp((int)vis.osc[0][i], 0, 255);
                    const int v1 = std::clamp((int)vis.osc[1][i], 0, 255);
                    faData[i] = (float)(((v0 - 128) / 2 + (v1 - 128) / 2 + 128) & 0xff);
                }
            }
            else
            {
                // center spectrum: uint8 average
                for (int i = 0; i < kAudioBins; i++)
                    faData[i] = (float)((int)vis.spec[0][i] / 2 + (int)vis.spec[1][i] / 2);
            }
        }
        else
        {
            const float* src = (Cfg.AudioSource == 0)
                             ? vis.osc[Cfg.AudioChannel]
                             : vis.spec[Cfg.AudioChannel];
            for (int i = 0; i < kAudioBins; i++)
                faData[i] = src[i];
        }
    }
    else
    {
        for (int i = 0; i < kAudioBins; i++) faData[i] = 128.0f;
    }

    // Radius scale from audio: sca ∈ [0.1, 1.0]
    // Waveform: XOR 128 maps silence→0, amplitude (both signs)→nonzero, making ring expand with loudness.
    // Spectrum: average two adjacent bins.
    auto getRadius = [&](int idx) -> float {
        if (Cfg.AudioSource == 0)
            return 0.1f + ((int(faData[idx]) ^ 128) / 255.0f) * 0.9f;
        else
        {
            const int si = idx * 2;
            return 0.1f + ((int(faData[si]) / 2 + int(faData[si + 1]) / 2) / 255.0f) * 0.9f;
        }
    };

    // Screen geometry
    const float fsize  = Cfg.Size / 32.0f;
    const float sizePx = std::min(H * fsize, W * fsize);
    const float cy     = (float)(H / 2);
    float cx;
    if      (Cfg.Position == 0) cx = (float)(W / 4);
    else if (Cfg.Position == 1) cx = (float)(W / 2 + W / 4);
    else                        cx = (float)(W / 2);

    // ── NanoVG overlay pass ───────────────────────────────────────────────────
    nvgluSetViewFramebuffer(Context.ViewId, m_overlayFbo);
    bgfx::setViewClear(Context.ViewId, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)W, (uint16_t)H);

    nvgluBindFramebuffer(m_overlayFbo);
    nvgBeginFrame(m_nvg, (float)W, (float)H, 1.0f);

    nvgStrokeColor(m_nvg, nvgRGBf(cr, cg, cb));
    nvgStrokeWidth(m_nvg, 1.0f);

    // Draw ring as 80 line segments.
    // Angle decrements (clockwise). Audio mirrored at q=40 so both halves are symmetric.
    float lx = std::trunc(cx + std::cos(0.0f) * sizePx * getRadius(0));
    float ly = std::trunc(cy + std::sin(0.0f) * sizePx * getRadius(0));
    float a = 0.0f;

    for (int q = 1; q <= 80; q++)
    {
        a -= kTwoPiOver80;

        // Mirrored audio: q=1..40 → bins 1..40, q=41..80 → bins 39..0
        const int idx = (q > 40) ? (80 - q) : q;
        const float sca = getRadius(idx);
        const float nx  = std::trunc(cx + std::cos(a) * sizePx * sca);
        const float ny  = std::trunc(cy + std::sin(a) * sizePx * sca);

        const bool lOn = (lx >= 0.0f && lx < W && ly >= 0.0f && ly < H);
        const bool nOn = (nx >= 0.0f && nx < W && ny >= 0.0f && ny < H);
        if (lOn || nOn)
        {
            nvgBeginPath(m_nvg);
            nvgMoveTo(m_nvg, lx, ly);
            nvgLineTo(m_nvg, nx, ny);
            nvgStroke(m_nvg);
        }

        lx = nx; ly = ny;
    }

    nvgEndFrame(m_nvg);
    nvgluBindFramebuffer(nullptr);

    // ── Composite pass ────────────────────────────────────────────────────────
    const uint8_t compView = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(compView, Context.OutputFBO);
    bgfx::setViewRect(compView, 0, 0, (uint16_t)W, (uint16_t)H);
    bgfx::setViewClear(compView, BGFX_CLEAR_NONE);

    const float params[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(m_paramsUniform, params);
    bgfx::setTexture(0, m_inputSampler,   Context.InputTexture);
    bgfx::setTexture(1, m_overlaySampler, bgfx::getTexture(m_overlayFbo->handle));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(compView, m_program);

    Context.FboManager->Swap();
}
