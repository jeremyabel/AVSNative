#include "Timescope.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_timescope.sc.bin.h"

#include <algorithm>

void Timescope::Init()
{
    const bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_timescope_spv,  sizeof(fs_timescope_spv)));
    m_program = bgfx::createProgram(VS, FS, true);
    m_uInput  = bgfx::createUniform("s_input",   bgfx::UniformType::Sampler);
    m_uColumn = bgfx::createUniform("s_column",  bgfx::UniformType::Sampler);
    m_uParams = bgfx::createUniform("u_tsParams", bgfx::UniformType::Vec4);
}

void Timescope::Destroy()
{
    if (bgfx::isValid(m_columnTex)) bgfx::destroy(m_columnTex);
    if (bgfx::isValid(m_uParams))   bgfx::destroy(m_uParams);
    if (bgfx::isValid(m_uColumn))   bgfx::destroy(m_uColumn);
    if (bgfx::isValid(m_uInput))    bgfx::destroy(m_uInput);
    if (bgfx::isValid(m_program))   bgfx::destroy(m_program);

    m_columnTex = BGFX_INVALID_HANDLE;
    m_uParams = m_uColumn = m_uInput = BGFX_INVALID_HANDLE;
    m_program = BGFX_INVALID_HANDLE;
}

void Timescope::EnsureScope(int w, int h)
{
    if (m_scopeW == w && m_scopeH == h) return;
    if (bgfx::isValid(m_columnTex)) bgfx::destroy(m_columnTex);

    m_scopeW = w;
    m_scopeH = h;
    m_position = 0;
    m_column.assign((size_t)h * 4, 0);

    // 1-wide, h-tall column. POINT so each row maps to exactly one frequency bin.
    m_columnTex = bgfx::createTexture2D(
        1, (uint16_t)h, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
}

void Timescope::Render(const RenderContext& Context)
{
    const int w = Context.Width, h = Context.Height;
    EnsureScope(w, h);

    const VisData* vd = Context.AudioData;

    // Advance the column position first (matches the original).
    m_position = (m_position + 1) % w;

    // Build this frame's scope column from the spectrum.
    for (int i = 0; i < h; i++)
    {
        int bin = (i * Bands) / h;          // integer indexing, exactly as the original
        bin = std::clamp(bin, 0, kAudioBins - 1);

        int val;
        if (Channel == 2)                    // Center = (L/2 + R/2)
            val = (vd ? (int)vd->spec[0][bin] / 2 + (int)vd->spec[1][bin] / 2 : 0);
        else
            val = (vd ? (int)vd->spec[std::clamp(Channel, 0, 1)][bin] : 0);
        val &= 0xFF;

        // color × magnitude / 256 (original fixed-point scaling).
        const size_t o = (size_t)i * 4;
        m_column[o]     = (uint8_t)((Color[0] * val) / 256);
        m_column[o + 1] = (uint8_t)((Color[1] * val) / 256);
        m_column[o + 2] = (uint8_t)((Color[2] * val) / 256);
        m_column[o + 3] = 255;
    }

    bgfx::updateTexture2D(m_columnTex, 0, 0, 0, 0, 1, (uint16_t)h,
                          bgfx::copy(m_column.data(), (uint32_t)m_column.size()));

    const float params[4] = { (float)m_position, (float)w, (float)Blend, 0.0f };
    bgfx::setUniform(m_uParams, params);
    bgfx::setTexture(0, m_uInput,  Context.InputTexture);
    bgfx::setTexture(1, m_uColumn, m_columnTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, m_program);

    Context.FboManager->Swap();
}

nlohmann::json Timescope::Serialize() const
{
    return {
        { kChannel, Channel },
        { kColor,   JsonUtil::ColorToJson(Color) },
        { kBlend,   Blend   },
        { kBands,   Bands   },
    };
}

void Timescope::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt  (j, kChannel, Channel);
    JsonUtil::ReadColor(j, kColor,   Color);
    JsonUtil::ReadInt  (j, kBlend,   Blend);
    JsonUtil::ReadInt  (j, kBands,   Bands);
}
