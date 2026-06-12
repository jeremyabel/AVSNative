#include "effects/Picture2.h"
#include "engine/JsonUtil.h"
#include "engine/FBOManager.h"

#include <bgfx/bgfx.h>
#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_picture2.sc.bin.h"

#include <vector>

// ── Serialize / Deserialize ───────────────────────────────────────────────────

nlohmann::json Picture2::Serialize() const
{
    return {
        { kBlendMode,         BlendMode         },
        { kAdjustBlend,       AdjustBlend       },
        { kBilinear,          Bilinear          },
        { kOnBeatBlendMode,   OnBeatBlendMode   },
        { kOnBeatAdjustBlend, OnBeatAdjustBlend },
        { kOnBeatBilinear,    OnBeatBilinear    },
        // Bundle asset reference — raw bytes arrive via ApplyAsset.
        { kImageData,         ImageData         },
    };
}

void Picture2::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt   (j, kBlendMode,         BlendMode);
    JsonUtil::ReadInt   (j, kAdjustBlend,       AdjustBlend);
    JsonUtil::ReadBool  (j, kBilinear,          Bilinear);
    JsonUtil::ReadInt   (j, kOnBeatBlendMode,   OnBeatBlendMode);
    JsonUtil::ReadInt   (j, kOnBeatAdjustBlend, OnBeatAdjustBlend);
    JsonUtil::ReadBool  (j, kOnBeatBilinear,    OnBeatBilinear);
    JsonUtil::ReadString(j, kImageData,         ImageData);
}

// ── Preset bundle assets ──────────────────────────────────────────────────────

std::vector<PresetAsset> Picture2::CollectAssets() const
{
    if (m_raw.empty())
        return {};
    return { { "imageData", m_name, m_raw } };
}

void Picture2::ApplyAsset(const std::string& /*key*/, const std::string& name,
                          std::vector<uint8_t> bytes)
{
    m_raw  = std::move(bytes);
    m_name = name;
    ImageData = name;
    if (m_inited)
        BuildFromRaw(m_raw);
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Picture2::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv,  sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_picture2_spv, sizeof(fs_picture2_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif  = bgfx::createUniform("s_input",    bgfx::UniformType::Sampler);
    m_imageUnif  = bgfx::createUniform("s_image",    bgfx::UniformType::Sampler);
    m_paramsUnif = bgfx::createUniform("u_p2Params", bgfx::UniformType::Vec4);

    m_inited = true;
    // No image until ApplyAsset delivers the bundled bytes (called after Deserialize).
}

void Picture2::Destroy()
{
    if (bgfx::isValid(m_imageTex))   bgfx::destroy(m_imageTex);
    if (bgfx::isValid(m_paramsUnif)) bgfx::destroy(m_paramsUnif);
    if (bgfx::isValid(m_imageUnif))  bgfx::destroy(m_imageUnif);
    if (bgfx::isValid(m_inputUnif))  bgfx::destroy(m_inputUnif);
    if (bgfx::isValid(m_prog))       bgfx::destroy(m_prog);

    m_imageTex   = BGFX_INVALID_HANDLE;
    m_paramsUnif = BGFX_INVALID_HANDLE;
    m_imageUnif  = BGFX_INVALID_HANDLE;
    m_inputUnif  = BGFX_INVALID_HANDLE;
    m_prog       = BGFX_INVALID_HANDLE;
    m_imgW = m_imgH = 0;
    m_inited = false;
}

// ── BuildFromRaw ──────────────────────────────────────────────────────────────

void Picture2::BuildFromRaw(const std::vector<uint8_t>& raw)
{
    if (bgfx::isValid(m_imageTex))
    {
        bgfx::destroy(m_imageTex);
        m_imageTex = BGFX_INVALID_HANDLE;
    }
    m_imgW = m_imgH = 0;

    if (raw.empty()) return;

    int w = 0, h = 0, ch = 0;
    uint8_t* pixels = stbi_load_from_memory(raw.data(), (int)raw.size(), &w, &h, &ch, 4);
    if (!pixels) return;

    m_imgW = w;
    m_imgH = h;

    const bgfx::Memory* mem = bgfx::copy(pixels, (uint32_t)(w * h * 4));
    stbi_image_free(pixels);

    // Sampler state is overridden per-draw by Render() for bilinear toggle.
    m_imageTex = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h,
        false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem);
}

// ── Render ────────────────────────────────────────────────────────────────────

void Picture2::Render(const RenderContext& Ctx)
{
    if (!bgfx::isValid(m_imageTex))
        return;  // No image loaded: pass through

    const int  blend  = Ctx.IsBeat() ? OnBeatBlendMode   : BlendMode;
    const bool linear = Ctx.IsBeat() ? OnBeatBilinear     : Bilinear;
    const int  adjust = Ctx.IsBeat() ? OnBeatAdjustBlend  : AdjustBlend;

    if (blend == 10)
        return;  // Ignore: pass through without swapping

    const uint32_t samplerFlags = linear
        ? (uint32_t)(BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP)
        : (uint32_t)(BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
                     BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP);

    float params[4] = { (float)blend, (float)adjust / 255.0f, 0.0f, 0.0f };

    bgfx::setTexture(0, m_inputUnif, Ctx.InputTexture);
    bgfx::setTexture(1, m_imageUnif, m_imageTex, samplerFlags);
    bgfx::setUniform(m_paramsUnif, params);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Ctx.QuadVB);
    bgfx::submit(Ctx.ViewId, m_prog);

    Ctx.FboManager->Swap();
}
