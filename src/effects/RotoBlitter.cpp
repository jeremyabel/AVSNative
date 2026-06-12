#include "RotoBlitter.h"
#include "engine/MathConstants.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_rotoblitter.sc.bin.h"

#include <algorithm>
#include <cmath>


void RotoBlitter::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_rotoblitter_spv, sizeof(fs_rotoblitter_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);
    
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    TransformUniform = bgfx::createUniform("u_rbTransform", bgfx::UniformType::Vec4);
    ResolutionUniform = bgfx::createUniform("u_rbResolution", bgfx::UniformType::Vec4);
}

void RotoBlitter::Render(const RenderContext& Context)
{
    // ── Rotation reversal ────────────────────────────────────────────────────
    if (Context.IsBeat() && Beatch)
        m_rotRev = -m_rotRev;
    if (!Beatch)
        m_rotRev = 1.0f;

    const float SpeedFactor = 1.0f / (1.0f + BeatchSpeed * 4.0f);
    m_rotRevPos += SpeedFactor * (m_rotRev - m_rotRevPos);
    if (m_rotRevPos > m_rotRev && m_rotRev > 0.0f)
        m_rotRevPos = m_rotRev;
    
    if (m_rotRevPos < m_rotRev && m_rotRev < 0.0f)
        m_rotRevPos = m_rotRev;

    // ── Scale animation ──────────────────────────────────────────────────────
    if (Context.IsBeat() && BeatchScale)
        m_scaleFpos = (float)ZoomScale2;

    float fVal;
    if (ZoomScale < ZoomScale2)
    {
        fVal = std::max(m_scaleFpos, (float)ZoomScale);
        if (m_scaleFpos > ZoomScale) m_scaleFpos -= 3.0f;
    }
    else
    {
        fVal = std::min(m_scaleFpos, (float)ZoomScale);
        if (m_scaleFpos < ZoomScale) m_scaleFpos += 3.0f;
    }

    // ── Transform ────────────────────────────────────────────────────────────
    const float zoom = 1.0f + (fVal - 31.0f) / 31.0f;
    const float thetaRad = (float)(RotDir - 32) * m_rotRevPos * avs::Pi / 180.0f;
    const float cosT = std::cos(thetaRad);
    const float sinT = std::sin(thetaRad);

    // ── Submit ───────────────────────────────────────────────────────────────
    const float Transform[4] = { cosT, sinT, zoom, Blend ? 1.0f : 0.0f };
    const float Resolution[4] = { (float)Context.Width, (float)Context.Height,
                                  Compat ? 1.0f : 0.0f, 0.0f };
    bgfx::setUniform(TransformUniform, Transform);
    bgfx::setUniform(ResolutionUniform, Resolution);

    // Compat does its own integer texelFetch blend, so bind POINT (the shader
    // ignores the hardware filter). Otherwise: bilinear when Subpixel, else nearest.
    const uint32_t PointFlags = BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
                                BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP;
    const uint32_t SamplerFlags = (Subpixel && !Compat) ? UINT32_MAX : PointFlags;
    bgfx::setTexture(0, TexUniform, Context.InputTexture, SamplerFlags);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void RotoBlitter::Destroy()
{
    if (bgfx::isValid(ResolutionUniform))
        bgfx::destroy(ResolutionUniform);
    
    if (bgfx::isValid(TransformUniform))
        bgfx::destroy(TransformUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ResolutionUniform = BGFX_INVALID_HANDLE;
    TransformUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json RotoBlitter::Serialize() const
{
    return {
        { kZoomScale,   ZoomScale   },
        { kZoomScale2,  ZoomScale2  },
        { kRotDir,      RotDir      },
        { kBeatchSpeed, BeatchSpeed },
        { kSubpixel,    Subpixel    },
        { kCompat,      Compat      },
        { kBlend,       Blend       },
        { kBeatch,      Beatch      },
        { kBeatchScale, BeatchScale },
    };
}

void RotoBlitter::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt (j, kZoomScale,   ZoomScale);
    JsonUtil::ReadInt (j, kZoomScale2,  ZoomScale2);
    JsonUtil::ReadInt (j, kRotDir,      RotDir);
    JsonUtil::ReadInt (j, kBeatchSpeed, BeatchSpeed);
    JsonUtil::ReadBool(j, kSubpixel,    Subpixel);
    JsonUtil::ReadBool(j, kCompat,      Compat);
    JsonUtil::ReadBool(j, kBlend,       Blend);
    JsonUtil::ReadBool(j, kBeatch,      Beatch);
    JsonUtil::ReadBool(j, kBeatchScale, BeatchScale);

    ResetZoomAnim();
}
