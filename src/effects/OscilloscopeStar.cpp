#include "OscilloscopeStar.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <nanovg/nanovg.h>
#include <nanovg/nanovg_bgfx.h>

#include <algorithm>
#include <cmath>

#include "engine/MathConstants.h"

static constexpr float kDfactorStart = 1.0f / 1024.0f;
static constexpr float kDfactorStep  = (kDfactorStart - 1.0f / 128.0f) / 64.0f;

// ── Init / Destroy ────────────────────────────────────────────────────────────

void OscilloscopeStar::Init()
{
    const bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_simple_spv,    sizeof(fs_simple_spv)));
    m_program = bgfx::createProgram(VS, FS, true);

    m_inputSampler   = bgfx::createUniform("s_input",        bgfx::UniformType::Sampler);
    m_overlaySampler = bgfx::createUniform("s_overlay",      bgfx::UniformType::Sampler);
    m_paramsUniform  = bgfx::createUniform("u_simpleParams",  bgfx::UniformType::Vec4);

    m_nvg = nvgCreate(1, 0);
}

void OscilloscopeStar::EnsureOverlay(int W, int H)
{
    if (m_overlayW == W && m_overlayH == H) return;
    DestroyOverlay();
    m_overlayFbo = nvgluCreateFramebuffer(m_nvg, W, H, 0);
    m_overlayW   = W;
    m_overlayH   = H;
}

void OscilloscopeStar::DestroyOverlay()
{
    if (m_overlayFbo) { nvgluDeleteFramebuffer(m_overlayFbo); m_overlayFbo = nullptr; }
    m_overlayW = m_overlayH = 0;
}

void OscilloscopeStar::Destroy()
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

void OscilloscopeStar::Render(const RenderContext& Context)
{
    if (Colors.empty()) return;

    const int W = Context.Width;
    const int H = Context.Height;

    EnsureOverlay(W, H);
    if (!m_overlayFbo) return;

    // Color cycling (same 64-step interpolation as RotatingStars / Simple)
    const int n = (int)Colors.size();
    m_colorPos = (m_colorPos + 1) % (n * 64);
    const int frac = m_colorPos & 63;
    const int seg  = m_colorPos / 64;
    const auto& c1 = Colors[seg % n];
    const auto& c2 = Colors[(seg + 1) % n];
    const float cr = (float)((c1[0] * (63 - frac) + c2[0] * frac) / 64) / 255.0f;
    const float cg = (float)((c1[1] * (63 - frac) + c2[1] * frac) / 64) / 255.0f;
    const float cb = (float)((c1[2] * (63 - frac) + c2[2] * frac) / 64) / 255.0f;

    // Audio data — waveform (osc) only, always; center = signed average of L and R
    float faData[kAudioBins] = {};
    if (Context.AudioData)
    {
        const VisData& vis = *Context.AudioData;
        if (AudioChannel == 2)
        {
            for (int i = 0; i < kAudioBins; i++)
            {
                const int v0 = std::clamp((int)vis.osc[0][i], 0, 255);
                const int v1 = std::clamp((int)vis.osc[1][i], 0, 255);
                faData[i] = (float)(((v0 - 128) / 2 + (v1 - 128) / 2 + 128) & 0xff);
            }
        }
        else
        {
            for (int i = 0; i < kAudioBins; i++)
                faData[i] = vis.osc[AudioChannel][i];
        }
    }
    else
    {
        for (int i = 0; i < kAudioBins; i++) faData[i] = 128.0f;
    }

    // Screen geometry
    const float fsize  = Size / 32.0f;
    const float sizePx = std::min(H * fsize, W * fsize);
    const float cy     = (float)(H / 2);
    float cx;
    if      (Position == 0) cx = (float)(W / 4);
    else if (Position == 1) cx = (float)(W / 2 + W / 4);
    else                        cx = (float)(W / 2);

    const float dp = sizePx / 64.0f;

    // ── NanoVG overlay pass ───────────────────────────────────────────────────
    nvgluSetViewFramebuffer(Context.ViewId, m_overlayFbo);
    bgfx::setViewClear(Context.ViewId, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)W, (uint16_t)H);

    nvgluBindFramebuffer(m_overlayFbo);
    nvgBeginFrame(m_nvg, (float)W, (float)H, 1.0f);

    nvgStrokeColor(m_nvg, nvgRGBf(cr, cg, cb));
    nvgStrokeWidth(m_nvg, 1.0f);

    // 5 arms, each 64 segments of waveform displaced perpendicular to arm direction.
    // Audio index ii is sequential across all arms (0..319), sampling osc data.
    int ii = 0;
    for (int q = 0; q < 5; q++)
    {
        const float angle = m_currentRotation + q * (avs::TwoPi / 5.0f);
        const float cosA  = std::cos(angle);
        const float sinA  = std::sin(angle);

        float dfactor = kDfactorStart;
        float p_dist  = 0.0f;

        // Each arm starts from the screen center (matches C++ `lx=c_x; ly=h/2`)
        float lx = cx, ly = cy;

        for (int s = 0; s < 64; s++, ii++)
        {
            const float ale = (faData[ii] - 128.0f) * dfactor * sizePx;
            const float nx  = std::trunc(cx + cosA * p_dist - sinA * ale);
            const float ny  = std::trunc(cy + sinA * p_dist + cosA * ale);

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
            p_dist  += dp;
            dfactor += kDfactorStep;
        }
    }

    // Advance rotation, wrapping in [0, 2π)
    m_currentRotation += 0.01f * (float)Rotation;
    while (m_currentRotation >= avs::TwoPi) m_currentRotation -= avs::TwoPi;
    while (m_currentRotation <  0.0f)       m_currentRotation += avs::TwoPi;

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

nlohmann::json OscilloscopeStar::Serialize() const
{
    return {
        { kColors,       JsonUtil::ColorsToJson(Colors) },
        { kAudioChannel, AudioChannel },
        { kPosition,     Position     },
        { kSize,         Size         },
        { kRotation,     Rotation     },
    };
}

void OscilloscopeStar::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColors(j, kColors,       Colors);
    JsonUtil::ReadInt   (j, kAudioChannel, AudioChannel);
    JsonUtil::ReadInt   (j, kPosition,     Position);
    JsonUtil::ReadInt   (j, kSize,         Size);
    JsonUtil::ReadInt   (j, kRotation,     Rotation);
}
