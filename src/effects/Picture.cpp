#include "effects/Picture.h"
#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include <bgfx/bgfx.h>
#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_picture.sc.bin.h"

#include <vector>

static constexpr const char* NAME_BlendMode = "blendMode";
static constexpr const char* NAME_EnableOnBeatAdditive = "onBeatAdditive";
static constexpr const char* NAME_OnBeatDuration = "onBeatDuration";
static constexpr const char* NAME_Fit = "fit";

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

void Picture::Init()
{
    bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_picture_spv,    sizeof(fs_picture_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    ImageUniform = bgfx::createUniform("s_image", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_picParams", bgfx::UniformType::Vec4);

    m_inited = true;
}

void Picture::BuildFromRaw(const std::vector<uint8_t>& raw)
{
    if (bgfx::isValid(ImageTexture))
    {
        bgfx::destroy(ImageTexture);
        ImageTexture = BGFX_INVALID_HANDLE;
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
    ImageTexture = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h,
        false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem);
}

void Picture::Render(const RenderContext& Context)
{
    if (!bgfx::isValid(ImageTexture))
        return;  // No image loaded: pass through without swapping (EffectChain handles it)

    if (Context.IsBeat() && OnBeatAdditive)
        m_cooldown = OnBeatDuration;
    else if (m_cooldown > 0)
        --m_cooldown;

    const bool beatActive    = Context.IsBeat() || m_cooldown > 0;
    const int  effectiveBlend =
        (BlendMode == 1 || (OnBeatAdditive && beatActive)) ? 1 : BlendMode;

    const float imgAspect = (m_imgH > 0) ? (float)m_imgW / (float)m_imgH : 1.0f;
    const float scrAspect = (Context.Height > 0) ? (float)Context.Width / (float)Context.Height : 1.0f;

    float uParams[4] = { (float)effectiveBlend, (float)Fit, imgAspect, scrAspect };

    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setTexture(1, ImageUniform, ImageTexture);
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Picture::Destroy()
{
    if (bgfx::isValid(ImageTexture))   
        bgfx::destroy(ImageTexture);

    if (bgfx::isValid(ParamsUniform)) 
       bgfx::destroy(ParamsUniform);

    if (bgfx::isValid(ImageUniform))  
        bgfx::destroy(ImageUniform);

    if (bgfx::isValid(TexUniform))  
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program))       
        bgfx::destroy(Program);

    ImageTexture = BGFX_INVALID_HANDLE;
    ParamsUniform = BGFX_INVALID_HANDLE;
    ImageUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
    m_imgW = m_imgH = 0;
    m_inited = false;
}

nlohmann::json Picture::Serialize() const
{
    return 
    {
        { NAME_BlendMode, BlendMode },
        { NAME_EnableOnBeatAdditive, OnBeatAdditive },
        { NAME_OnBeatDuration, OnBeatDuration },
        { NAME_Fit, Fit },
        // Bundle asset reference — raw bytes arrive via ApplyAsset.
        { NAME_ImageData, ImageData },
    };
}

void Picture::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_BlendMode, BlendMode);
    JsonUtil::ReadBool(j, NAME_EnableOnBeatAdditive, OnBeatAdditive);
    JsonUtil::ReadInt(j, NAME_OnBeatDuration, OnBeatDuration);
    JsonUtil::ReadInt(j, NAME_Fit, Fit);
    JsonUtil::ReadString(j, NAME_ImageData, ImageData);
}
