#include "effects/Picture.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include <bgfx/bgfx.h>
#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_picture.sc.bin.h"

#include <vector>

// ── Serialize / Deserialize ───────────────────────────────────────────────────

nlohmann::json Picture::Serialize() const
{
    return {
        { kBlendMode,      BlendMode      },
        { kOnBeatAdditive, OnBeatAdditive },
        { kOnBeatDuration, OnBeatDuration },
        { kFit,            Fit            },
        // Bundle asset reference — raw bytes arrive via ApplyAsset.
        { kImageData,      ImageData      },
    };
}

void Picture::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt   (j, kBlendMode,      BlendMode);
    JsonUtil::ReadBool  (j, kOnBeatAdditive, OnBeatAdditive);
    JsonUtil::ReadInt   (j, kOnBeatDuration, OnBeatDuration);
    JsonUtil::ReadInt   (j, kFit,            Fit);
    JsonUtil::ReadString(j, kImageData,      ImageData);
}

// ── Preset bundle assets ──────────────────────────────────────────────────────

std::vector<PresetAsset> Picture::CollectAssets() const
{
    if (m_raw.empty())
        return {};
    return { { "imageData", m_name, m_raw } };
}

void Picture::ApplyAsset(const std::string& /*key*/, const std::string& name,
                         std::vector<uint8_t> bytes)
{
    m_raw  = std::move(bytes);
    m_name = name;
    ImageData = name;  // non-empty marker for round-trip
    if (m_inited)
        BuildFromRaw(m_raw);
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Picture::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_picture_spv,    sizeof(fs_picture_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif  = bgfx::createUniform("s_input",     bgfx::UniformType::Sampler);
    m_imageUnif  = bgfx::createUniform("s_image",     bgfx::UniformType::Sampler);
    m_paramsUnif = bgfx::createUniform("u_picParams", bgfx::UniformType::Vec4);

    m_inited = true;
    // No image until ApplyAsset delivers the bundled bytes (called after Deserialize).
}

void Picture::Destroy()
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

void Picture::BuildFromRaw(const std::vector<uint8_t>& raw)
{
    if (bgfx::isValid(m_imageTex))
    {
        bgfx::destroy(m_imageTex);
        m_imageTex = BGFX_INVALID_HANDLE;
    }
    m_imgW = m_imgH = 0;

    if (raw.empty()) return;

    int w = 0, h = 0, ch = 0;
    uint8_t* pixels = stbi_load_from_memory(
        raw.data(), (int)raw.size(), &w, &h, &ch, 4);
    if (!pixels) return;

    m_imgW = w;
    m_imgH = h;

    const bgfx::Memory* mem = bgfx::copy(pixels, (uint32_t)(w * h * 4));
    stbi_image_free(pixels);

    // No Y-flip: stb_image row 0 = top, bgfx/Vulkan UV (0,0) = top-left. Match. ✓
    // Default bgfx sampler filter is linear; U/V clamp prevents edge artifacts.
    m_imageTex = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h,
        false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem);
}

// ── Render ────────────────────────────────────────────────────────────────────

void Picture::Render(const RenderContext& Ctx)
{
    if (!bgfx::isValid(m_imageTex))
        return;  // No image loaded: pass through without swapping (EffectChain handles it)

    if (Ctx.IsBeat() && OnBeatAdditive)
        m_cooldown = OnBeatDuration;
    else if (m_cooldown > 0)
        --m_cooldown;

    const bool beatActive    = Ctx.IsBeat() || m_cooldown > 0;
    const int  effectiveBlend =
        (BlendMode == 1 || (OnBeatAdditive && beatActive)) ? 1 : BlendMode;

    const float imgAspect = (m_imgH > 0) ? (float)m_imgW / (float)m_imgH : 1.0f;
    const float scrAspect = (Ctx.Height > 0) ? (float)Ctx.Width / (float)Ctx.Height : 1.0f;

    float params[4] = {
        (float)effectiveBlend,
        (float)Fit,
        imgAspect,
        scrAspect,
    };

    bgfx::setTexture(0, m_inputUnif,  Ctx.InputTexture);
    bgfx::setTexture(1, m_imageUnif,  m_imageTex);
    bgfx::setUniform(m_paramsUnif, params);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Ctx.QuadVB);
    bgfx::submit(Ctx.ViewId, m_prog);

    Ctx.FboManager->Swap();
}
