#include "effects/Ramp.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include <bgfx/bgfx.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_ramp.sc.bin.h"

#include <algorithm>
#include <cmath>

// ── Baking ─────────────────────────────────────────────────────────────────────

void Ramp::Bake()
{
    uint8_t* lut = m_baked.data();

    std::vector<RampStop> sorted = Stops;
    if (sorted.empty())
    {
        for (int i = 0; i < kLutSize * 4; ++i) lut[i] = (i % 4 == 3) ? 255 : 0;
        return;
    }
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const RampStop& a, const RampStop& b) {
                         if (a.Position != b.Position) return a.Position < b.Position;
                         return (a.Color[0] + a.Color[1] + a.Color[2])
                              < (b.Color[0] + b.Color[1] + b.Color[2]);
                     });

    const RampStop& first = sorted.front();
    const int firstPos = std::clamp(first.Position, 0, kLutSize);
    for (int i = 0; i < firstPos; ++i) {
        lut[i*4+0] = first.Color[0];
        lut[i*4+1] = first.Color[1];
        lut[i*4+2] = first.Color[2];
        lut[i*4+3] = 255;
    }

    for (size_t ci = 0; ci + 1 < sorted.size(); ++ci) {
        const RampStop& from = sorted[ci];
        const RampStop& to   = sorted[ci + 1];
        const int fp = std::clamp(from.Position, 0, kLutSize);
        const int tp = std::clamp(to.Position,   0, kLutSize);
        const int span = tp - fp;
        for (int i = fp; i < tp; ++i) {
            const float t = span > 0 ? (float)(i - fp) / (float)span : 0.0f;
            lut[i*4+0] = (uint8_t)(from.Color[0] + (to.Color[0] - from.Color[0]) * t);
            lut[i*4+1] = (uint8_t)(from.Color[1] + (to.Color[1] - from.Color[1]) * t);
            lut[i*4+2] = (uint8_t)(from.Color[2] + (to.Color[2] - from.Color[2]) * t);
            lut[i*4+3] = 255;
        }
    }

    const RampStop& last = sorted.back();
    const int lastPos = std::clamp(last.Position, 0, kLutSize);
    for (int i = lastPos; i < kLutSize; ++i) {
        lut[i*4+0] = last.Color[0];
        lut[i*4+1] = last.Color[1];
        lut[i*4+2] = last.Color[2];
        lut[i*4+3] = 255;
    }
}

// ── Serialize / Deserialize ───────────────────────────────────────────────────

nlohmann::json Ramp::Serialize() const
{
    nlohmann::json colors = nlohmann::json::array();
    for (const RampStop& s : Stops)
        colors.push_back({ { "position", s.Position },
                           { "color", { s.Color[0], s.Color[1], s.Color[2] } } });

    return {
        { kType,            Type            },
        { kScale,           Scale           },
        { kRotation,        Rotation        },
        { kBlendMode,       BlendMode       },
        { kUseWindowAspect, UseWindowAspect },
        { kColors,          colors          },
    };
}

void Ramp::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt  (j, kType,            Type);
    JsonUtil::ReadFloat(j, kScale,           Scale);
    JsonUtil::ReadFloat(j, kRotation,        Rotation);
    JsonUtil::ReadInt  (j, kBlendMode,       BlendMode);
    JsonUtil::ReadBool (j, kUseWindowAspect, UseWindowAspect);

    if (j.contains(kColors) && j[kColors].is_array())
    {
        std::vector<RampStop> stops;
        for (const auto& c : j[kColors])
        {
            RampStop s;
            s.Position = c.value("position", 0);
            if (c.contains("color") && c["color"].is_array() && c["color"].size() == 3)
            {
                const auto& col = c["color"];
                s.Color = { (uint8_t)col[0].get<int>(),
                            (uint8_t)col[1].get<int>(),
                            (uint8_t)col[2].get<int>() };
            }
            stops.push_back(s);
        }
        if (!stops.empty())
            Stops = std::move(stops);
    }

    Bake();
    ++m_configVersion;
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Ramp::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_ramp_spv,       sizeof(fs_ramp_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif   = bgfx::createUniform("s_input",       bgfx::UniformType::Sampler);
    m_lutUnif     = bgfx::createUniform("s_lut",         bgfx::UniformType::Sampler);
    m_paramsUnif  = bgfx::createUniform("u_rampParams",  bgfx::UniformType::Vec4);
    m_paramsUnif2 = bgfx::createUniform("u_rampParams2", bgfx::UniformType::Vec4);

    m_lutTex = bgfx::createTexture2D(
        kLutSize, 1, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
        BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP);

    Bake();
}

void Ramp::Destroy()
{
    if (bgfx::isValid(m_lutTex))      bgfx::destroy(m_lutTex);
    if (bgfx::isValid(m_paramsUnif2)) bgfx::destroy(m_paramsUnif2);
    if (bgfx::isValid(m_paramsUnif))  bgfx::destroy(m_paramsUnif);
    if (bgfx::isValid(m_lutUnif))     bgfx::destroy(m_lutUnif);
    if (bgfx::isValid(m_inputUnif))   bgfx::destroy(m_inputUnif);
    if (bgfx::isValid(m_prog))        bgfx::destroy(m_prog);

    m_lutTex      = BGFX_INVALID_HANDLE;
    m_paramsUnif2 = BGFX_INVALID_HANDLE;
    m_paramsUnif  = BGFX_INVALID_HANDLE;
    m_lutUnif     = BGFX_INVALID_HANDLE;
    m_inputUnif   = BGFX_INVALID_HANDLE;
    m_prog        = BGFX_INVALID_HANDLE;
}

// ── Render ────────────────────────────────────────────────────────────────────

void Ramp::Render(const RenderContext& Ctx)
{
    bgfx::updateTexture2D(m_lutTex, 0, 0, 0, 0, kLutSize, 1,
                          bgfx::copy(m_baked.data(), kLutSize * 4));

    const float aspect = (UseWindowAspect && Ctx.Height > 0)
                       ? (float)Ctx.Width / (float)Ctx.Height : 1.0f;
    const float rot    = Rotation * 3.14159265358979f / 180.0f;

    float params[4]  = { (float)Type, Scale, rot, (float)BlendMode };
    float params2[4] = { aspect, 0.0f, 0.0f, 0.0f };

    bgfx::setTexture(0, m_inputUnif, Ctx.InputTexture);
    bgfx::setTexture(1, m_lutUnif,   m_lutTex);
    bgfx::setUniform(m_paramsUnif,  params);
    bgfx::setUniform(m_paramsUnif2, params2);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Ctx.QuadVB);
    bgfx::submit(Ctx.ViewId, m_prog);

    Ctx.FboManager->Swap();
}
