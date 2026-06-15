#include "DotFountain.h"

#include "engine/JsonUtil.h"
#include "engine/MathConstants.h"
#include "engine/Matrix3D.h"
#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <algorithm>
#include <cmath>

static constexpr const char* NAME_RotationSpeed = "rotationSpeed";
static constexpr const char* NAME_Angle = "angle";
static constexpr const char* NAME_Color0 = "color0";
static constexpr const char* NAME_Color1 = "color1";
static constexpr const char* NAME_Color2 = "color2";
static constexpr const char* NAME_Color3 = "color3";
static constexpr const char* NAME_Color4 = "color4";

void DotFountain::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_simple_spv, sizeof(fs_simple_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);
    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    OverlayUniform = bgfx::createUniform("s_overlay", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_simpleParams", bgfx::UniformType::Vec4);

    BuildColorMap();
}

void DotFountain::Destroy()
{
    if (bgfx::isValid(OverlayTexture)) 
        bgfx::destroy(OverlayTexture);

    if (bgfx::isValid(ParamsUniform))    
        bgfx::destroy(ParamsUniform);

    if (bgfx::isValid(OverlayUniform))   
        bgfx::destroy(OverlayUniform);

    if (bgfx::isValid(TexUniform))      
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program))    
        bgfx::destroy(Program);

    OverlayTexture = BGFX_INVALID_HANDLE;
    ParamsUniform = BGFX_INVALID_HANDLE;
    OverlayUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void DotFountain::BuildColorMap()
{
    const std::array<uint8_t, 3>* cols[5] = { &Color0, &Color1, &Color2, &Color3, &Color4 };
    for (int t = 0; t < 4; t++)
    {
        const auto& c1 = *cols[t];
        const auto& c2 = *cols[t + 1];
        // Fixed-point interpolation matching the original (<<16 fraction).
        int32_t r = c1[0] << 16, g = c1[1] << 16, b = c1[2] << 16;
        const int32_t dr = (((int)c2[0] - c1[0]) << 16) / 16;
        const int32_t dg = (((int)c2[1] - c1[1]) << 16) / 16;
        const int32_t db = (((int)c2[2] - c1[2]) << 16) / 16;
        for (int x = 0; x < 16; x++)
        {
            m_mapR[t * 16 + x] = (r >> 16) & 0xff;
            m_mapG[t * 16 + x] = (g >> 16) & 0xff;
            m_mapB[t * 16 + x] = (b >> 16) & 0xff;
            r += dr; g += dg; b += db;
        }
    }
}

void DotFountain::EnsureOverlay(int w, int h)
{
    if (OverlayW == w && OverlayH == h) return;
    if (bgfx::isValid(OverlayTexture)) bgfx::destroy(OverlayTexture);
    OverlayW = w;
    OverlayH = h;
    OverlayBuffer.assign((size_t)w * h * 4, 0);
    OverlayTexture = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
}

void DotFountain::Render(const RenderContext& Context)
{
    const int w = Context.Width, h = Context.Height;
    EnsureOverlay(w, h);

    const VisData* vd = Context.AudioData;
    const bool isBeat = Context.IsBeat();

    // ── 1. Shift generations 254→255 … 0→1 (high-to-low), applying physics. ──────
    for (int gen = NUM_GENS - 2; gen >= 0; gen--)
    {
        const float accelRadius = 1.3f / (gen + 100);
        const int srcBase = gen * NUM_ANG;
        const int dstBase = (gen + 1) * NUM_ANG;
        for (int a = 0; a < NUM_ANG; a++)
        {
            const int src = srcBase + a, dst = dstBase + a;
            m_rad[dst]  = m_rad[src];
            m_dRad[dst] = m_dRad[src];
            m_ht[dst]   = m_ht[src];
            m_dHt[dst]  = m_dHt[src];
            m_ax[dst]   = m_ax[src];
            m_ay[dst]   = m_ay[src];
            m_colR[dst] = m_colR[src];
            m_colG[dst] = m_colG[src];
            m_colB[dst] = m_colB[src];
            // Physics on the destination.
            m_rad[dst]  += m_dRad[dst];
            m_dHt[dst]  += 0.05f;
            m_dRad[dst] += accelRadius;
            m_ht[dst]   += m_dHt[dst];
        }
    }

    // ── 2. Spawn generation 0 from the waveform. ─────────────────────────────────
    for (int a = 0; a < NUM_ANG; a++)
    {
        const float sample = vd ? vd->osc[0][a] : 128.0f;   // uint8 0..255, 128 = silence
        int audio = (int)std::trunc(sample * 5.0f / 4.0f) - 64 + (isBeat ? 128 : 0);
        if (audio > 255) audio = 255;

        m_rad[a] = 1.0f;
        m_ht[a]  = 250.0f;
        const float dr = std::abs((float)audio) / 200.0f + 1.0f;
        m_dHt[a]  = -dr * 2.8f;
        m_dRad[a] = 0.0f;

        const int colorIdx = std::clamp((int)std::trunc(audio / 4.0f), 0, 63);
        m_colR[a] = m_mapR[colorIdx];
        m_colG[a] = m_mapG[colorIdx];
        m_colB[a] = m_mapB[colorIdx];

        const float angle = a * avs::Pi * 2.0f / NUM_ANG;
        m_ax[a] = std::sin(angle);
        m_ay[a] = std::cos(angle);
    }

    // ── 3. Build transform: T(0,-20,400) × Rx(angle) × Ry(rotation). ─────────────
    float m[16], m2[16];
    avs::MatRot(m,  2, CurrentRotation);
    avs::MatRot(m2, 1, (float)Angle);
    avs::MatMul(m, m2);
    avs::MatTrans(m2, 0.0f, -20.0f, 400.0f);
    avs::MatMul(m, m2);

    // ── 4. Project & additively render into the overlay buffer. ──────────────────
    std::fill(OverlayBuffer.begin(), OverlayBuffer.end(), (uint8_t)0);

    float zoom = w * 440.0f / 640.0f;
    const float zoom2 = h * 440.0f / 480.0f;
    if (zoom2 < zoom) zoom = zoom2;

    const int hw = w / 2, hh = h / 2;

    for (int i = 0; i < N; i++)
    {
        const float px = m_ax[i] * m_rad[i];
        const float py = m_ht[i];
        const float pz = m_ay[i] * m_rad[i];

        const float ox = px*m[0] + py*m[1] + pz*m[2]  + m[3];
        const float oy = px*m[4] + py*m[5] + pz*m[6]  + m[7];
        const float oz = px*m[8] + py*m[9] + pz*m[10] + m[11];

        if (oz > 0.0000001f)
        {
            const float zp = zoom / oz;
            const int sx = (int)(ox * zp) + hw;
            const int sy = (int)(oy * zp) + hh;   // top-down screen y
            if (sx >= 0 && sx < w && sy >= 0 && sy < h)
            {
                // bgfx textures are top-left origin — write row sy directly (no flip).
                const size_t idx = ((size_t)sy * w + sx) * 4;
                OverlayBuffer[idx]     = (uint8_t)std::min(255, OverlayBuffer[idx]     + m_colR[i]);
                OverlayBuffer[idx + 1] = (uint8_t)std::min(255, OverlayBuffer[idx + 1] + m_colG[i]);
                OverlayBuffer[idx + 2] = (uint8_t)std::min(255, OverlayBuffer[idx + 2] + m_colB[i]);
                OverlayBuffer[idx + 3] = 255;
            }
        }
    }

    // ── 5. Upload overlay & composite additively over the input. ─────────────────
    bgfx::updateTexture2D(OverlayTexture, 0, 0, 0, 0, (uint16_t)w, (uint16_t)h,
                          bgfx::copy(OverlayBuffer.data(), (uint32_t)OverlayBuffer.size()));

    const float uParams[4] = { 1.0f, 0.0f, 0.0f, 0.0f };   // fs_simple mode 1 = additive
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setTexture(1, OverlayUniform, OverlayTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();

    // ── 6. Advance rotation. ─────────────────────────────────────────────────────
    CurrentRotation += RotationSpeed / 5.0f;
    if (CurrentRotation >= 360.0f) CurrentRotation -= 360.0f;
    if (CurrentRotation <    0.0f) CurrentRotation += 360.0f;
}

nlohmann::json DotFountain::Serialize() const
{
    return 
    {
        { NAME_RotationSpeed, RotationSpeed },
        { NAME_Angle, Angle },
        { NAME_Color0, JsonUtil::ColorToJson(Color0) },
        { NAME_Color1, JsonUtil::ColorToJson(Color1) },
        { NAME_Color2, JsonUtil::ColorToJson(Color2) },
        { NAME_Color3, JsonUtil::ColorToJson(Color3) },
        { NAME_Color4, JsonUtil::ColorToJson(Color4) },
    };
}

void DotFountain::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_RotationSpeed, RotationSpeed);
    JsonUtil::ReadInt(j, NAME_Angle, Angle);
    JsonUtil::ReadColor(j, NAME_Color0, Color0);
    JsonUtil::ReadColor(j, NAME_Color1, Color1);
    JsonUtil::ReadColor(j, NAME_Color2, Color2);
    JsonUtil::ReadColor(j, NAME_Color3, Color3);
    JsonUtil::ReadColor(j, NAME_Color4, Color4);

    BuildColorMap();
}
