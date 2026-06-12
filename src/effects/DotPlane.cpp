#include "DotPlane.h"

#include "engine/JsonUtil.h"
#include "engine/MathConstants.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_dotplane.sc.bin.h"

#include <algorithm>
#include <cmath>
#include <cstring>


// ── Matrix helpers (ported from matrix.cpp). ──────────────────────────────────

static void MatRot(float* m, int axis, float deg)
{
    const float r = deg * avs::Pi / 180.0f;
    std::fill(m, m + 16, 0.0f);
    m[(axis - 1) * 4 + (axis - 1)] = 1.0f;
    m[15] = 1.0f;
    const int m1 = axis % 3;
    const int m2 = (m1 + 1) % 3;
    const float c = std::cos(r), s = std::sin(r);
    m[m1 * 4 + m1] = c;
    m[m1 * 4 + m2] = s;
    m[m2 * 4 + m2] = c;
    m[m2 * 4 + m1] = -s;
}

static void MatTrans(float* m, float x, float y, float z)
{
    std::fill(m, m + 16, 0.0f);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    m[3] = x; m[7] = y; m[11] = z;
}

// dest = src × dest_old
static void MatMul(float* dest, const float* src)
{
    float t[16];
    std::memcpy(t, dest, sizeof(t));
    for (int i = 0; i < 16; i += 4)
    {
        dest[i]     = src[i]*t[0] + src[i+1]*t[4] + src[i+2]*t[8]  + src[i+3]*t[12];
        dest[i + 1] = src[i]*t[1] + src[i+1]*t[5] + src[i+2]*t[9]  + src[i+3]*t[13];
        dest[i + 2] = src[i]*t[2] + src[i+1]*t[6] + src[i+2]*t[10] + src[i+3]*t[14];
        dest[i + 3] = src[i]*t[3] + src[i+1]*t[7] + src[i+2]*t[11] + src[i+3]*t[15];
    }
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void DotPlane::Init()
{
    const bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_dotplane_spv,  sizeof(fs_dotplane_spv)));
    m_program = bgfx::createProgram(VS, FS, true);
    m_uInput  = bgfx::createUniform("s_input",   bgfx::UniformType::Sampler);
    m_uDots   = bgfx::createUniform("s_overlay", bgfx::UniformType::Sampler);

    BuildColorMap();
}

void DotPlane::Destroy()
{
    if (bgfx::isValid(m_dotsTex)) bgfx::destroy(m_dotsTex);
    if (bgfx::isValid(m_uDots))   bgfx::destroy(m_uDots);
    if (bgfx::isValid(m_uInput))  bgfx::destroy(m_uInput);
    if (bgfx::isValid(m_program)) bgfx::destroy(m_program);

    m_dotsTex = BGFX_INVALID_HANDLE;
    m_uDots = m_uInput = BGFX_INVALID_HANDLE;
    m_program = BGFX_INVALID_HANDLE;
}

void DotPlane::BuildColorMap()
{
    const std::array<uint8_t, 3>* cols[5] = { &Color0, &Color1, &Color2, &Color3, &Color4 };
    for (int t = 0; t < 4; t++)
    {
        const auto& c1 = *cols[t];
        const auto& c2 = *cols[t + 1];
        for (int x = 0; x < 16; x++)
        {
            const int i = (t * 16 + x) * 3;
            m_colorMap[i]     = (uint8_t)(int)(c1[0] + x * ((int)c2[0] - c1[0]) / 16.0f);
            m_colorMap[i + 1] = (uint8_t)(int)(c1[1] + x * ((int)c2[1] - c1[1]) / 16.0f);
            m_colorMap[i + 2] = (uint8_t)(int)(c1[2] + x * ((int)c2[2] - c1[2]) / 16.0f);
        }
    }
}

void DotPlane::EnsureDots(int w, int h)
{
    if (m_dotsW == w && m_dotsH == h) return;
    if (bgfx::isValid(m_dotsTex)) bgfx::destroy(m_dotsTex);
    m_dotsW = w;
    m_dotsH = h;
    m_buf.assign((size_t)w * h * 4, 0);
    m_dotsTex = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
}

// Scroll the grid back and push a new spectrum row into row 0.
void DotPlane::UpdateGrid(const VisData* vd)
{
    // Save row 0 before modification (used for the new-row delta).
    for (int x = 0; x < GRID; x++) m_tmp[x] = m_height[x];

    // Propagate row[line+1] ← decayed(row[line]), iterating from row 62 down to row 0.
    for (int yp = 0, line = (GRID - 2) * GRID; yp < GRID - 1; yp++, line -= GRID)
    {
        for (int x = 0; x < GRID; x++)
        {
            float nh = m_height[line + x] + m_delta[line + x];
            if (nh < 0.0f) nh = 0.0f;
            m_height[line + GRID + x] = nh;
            m_delta[line + GRID + x]  = m_delta[line + x] - 0.15f * (nh / 255.0f);
            m_color[line + GRID + x]  = m_color[line + x];
        }
    }

    // Push the new audio line into row 0 (spectrum, channel 0).
    const float* spec = vd ? vd->spec[0] : nullptr;
    const auto sp = [&](int i) -> int { return spec ? (int)spec[std::min(i, kAudioBins - 1)] : 0; };
    for (int x = 0; x < GRID; x++)
    {
        const int i0 = x * 3;
        int audio = std::max({ sp(i0), sp(i0 + 1), sp(i0 + 2) });
        if (audio > 255) audio = 255;
        m_height[x] = (float)audio;
        m_color[x]  = (uint8_t)std::min(63, audio / 4);
        m_delta[x]  = (audio - m_tmp[x]) / 90.0f;
    }
}

// ── Render ────────────────────────────────────────────────────────────────────

void DotPlane::Render(const RenderContext& Context)
{
    const int sw = Context.Width, sh = Context.Height;
    EnsureDots(sw, sh);

    // Build 3D transform: T(0,-20,400) × Rx(angle) × Ry(rotation).
    float m[16], m2[16];
    MatRot(m,  2, m_rotation);
    MatRot(m2, 1, (float)Angle);
    MatMul(m, m2);
    MatTrans(m2, 0.0f, -20.0f, 400.0f);
    MatMul(m, m2);

    UpdateGrid(Context.AudioData);

    const float zoom = std::min(sw * 440.0f / 640.0f, sh * 440.0f / 480.0f);

    std::fill(m_buf.begin(), m_buf.end(), (uint8_t)0);

    const float rot = m_rotation;
    for (int yp = 0; yp < GRID; yp++)
    {
        // Painter's order: pick the grid row to draw by rotation quadrant.
        const int gridRow = (rot < 90.0f || rot > 270.0f) ? GRID - yp - 1 : yp;

        const float gridStep0 = 350.0f / GRID;
        float gridStep = gridStep0;
        float curY = -(GRID * 0.5f) * gridStep0;
        const float curX = (gridRow - GRID * 0.5f) * gridStep0;

        int colBase = gridRow * GRID;
        int dir = 1;

        if (rot < 180.0f)
        {
            dir = -1;
            gridStep = -gridStep0;
            curY = -curY + gridStep;
            colBase += GRID - 1;
        }

        for (int xp = 0; xp < GRID; xp++)
        {
            const float ph = m_height[colBase];
            const float vx = curY, vy = 64.0f - ph, vz = curX;
            const float ox = vx*m[0] + vy*m[1] + vz*m[2]  + m[3];
            const float oy = vx*m[4] + vy*m[5] + vz*m[6]  + m[7];
            const float oz = vx*m[8] + vy*m[9] + vz*m[10] + m[11];

            if (oz > 0.0f)
            {
                const float iz = zoom / oz;
                const int sx = (int)(ox * iz + sw * 0.5f);
                const int sy = (int)(oy * iz + sh * 0.5f);
                if (sx >= 0 && sx < sw && sy >= 0 && sy < sh)
                {
                    const int ci = m_color[colBase] * 3;
                    // bgfx textures are top-left origin — write row sy directly (no flip).
                    const size_t bi = ((size_t)sy * sw + sx) * 4;
                    m_buf[bi]     = m_colorMap[ci];
                    m_buf[bi + 1] = m_colorMap[ci + 1];
                    m_buf[bi + 2] = m_colorMap[ci + 2];
                    m_buf[bi + 3] = 255;
                }
            }
            curY    += gridStep;
            colBase += dir;
        }
    }

    // Advance rotation.
    m_rotation += RotationSpeed / 5.0f;
    if (m_rotation >= 360.0f) m_rotation -= 360.0f;
    if (m_rotation <    0.0f) m_rotation += 360.0f;

    // Upload dots & composite (screen blend) over the input.
    bgfx::updateTexture2D(m_dotsTex, 0, 0, 0, 0, (uint16_t)sw, (uint16_t)sh,
                          bgfx::copy(m_buf.data(), (uint32_t)m_buf.size()));

    bgfx::setTexture(0, m_uInput, Context.InputTexture);
    bgfx::setTexture(1, m_uDots,  m_dotsTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, m_program);

    Context.FboManager->Swap();
}

nlohmann::json DotPlane::Serialize() const
{
    return {
        { kRotationSpeed, RotationSpeed },
        { kAngle,         Angle         },
        { kColor0,        JsonUtil::ColorToJson(Color0) },
        { kColor1,        JsonUtil::ColorToJson(Color1) },
        { kColor2,        JsonUtil::ColorToJson(Color2) },
        { kColor3,        JsonUtil::ColorToJson(Color3) },
        { kColor4,        JsonUtil::ColorToJson(Color4) },
    };
}

void DotPlane::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt  (j, kRotationSpeed, RotationSpeed);
    JsonUtil::ReadInt  (j, kAngle,         Angle);
    JsonUtil::ReadColor(j, kColor0,        Color0);
    JsonUtil::ReadColor(j, kColor1,        Color1);
    JsonUtil::ReadColor(j, kColor2,        Color2);
    JsonUtil::ReadColor(j, kColor3,        Color3);
    JsonUtil::ReadColor(j, kColor4,        Color4);

    BuildColorMap();
}
