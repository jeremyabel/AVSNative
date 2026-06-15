#include "effects/Picture2.h"
#include "engine/JsonUtil.h"
#include "engine/FBOManager.h"

#include <bgfx/bgfx.h>
#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_picture2.sc.bin.h"

#include <vector>

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

void Picture2::Init()
{
    bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv,  sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_picture2_spv, sizeof(fs_picture2_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    ImageUniform = bgfx::createUniform("s_image", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_p2Params", bgfx::UniformType::Vec4);

    m_inited = true;
}

void Picture2::BuildFromRaw(const std::vector<uint8_t>& raw)
{
    if (bgfx::isValid(ImageTexture))
    {
        bgfx::destroy(ImageTexture);
        ImageTexture = BGFX_INVALID_HANDLE;
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
    ImageTexture = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h,
        false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem);
}

void Picture2::Render(const RenderContext& Context)
{
    if (!bgfx::isValid(ImageTexture))
        return;  // No image loaded: pass through

    const int blend  = Context.IsBeat() ? OnBeatBlendMode : BlendMode;
    const bool linear = Context.IsBeat() ? OnBeatBilinear : Bilinear;
    const int adjust = Context.IsBeat() ? OnBeatAdjustBlend : AdjustBlend;

    if (blend == 10)
        return;  // Ignore: pass through without swapping

    const uint32_t SamplerFlags = linear
        ? (uint32_t)(BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP)
        : (uint32_t)(BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
                     BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP);

    float uParams[4] = { (float)blend, (float)adjust / 255.0f, 0.0f, 0.0f };

    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setTexture(1, ImageUniform, ImageTexture, SamplerFlags);
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Picture2::Destroy()
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

nlohmann::json Picture2::Serialize() const
{
    return 
    {
        { kBlendMode, BlendMode },
        { kAdjustBlend, AdjustBlend },
        { kBilinear, Bilinear },
        { kOnBeatBlendMode, OnBeatBlendMode },
        { kOnBeatAdjustBlend, OnBeatAdjustBlend },
        { kOnBeatBilinear, OnBeatBilinear },
        { kImageData, ImageData }, // Bundle asset reference — raw bytes arrive via ApplyAsset.
    };
}

void Picture2::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, kBlendMode, BlendMode);
    JsonUtil::ReadInt(j, kAdjustBlend, AdjustBlend);
    JsonUtil::ReadBool(j, kBilinear, Bilinear);
    JsonUtil::ReadInt(j, kOnBeatBlendMode, OnBeatBlendMode);
    JsonUtil::ReadInt(j, kOnBeatAdjustBlend, OnBeatAdjustBlend);
    JsonUtil::ReadBool(j, kOnBeatBilinear, OnBeatBilinear);
    JsonUtil::ReadString(j, kImageData, ImageData);
}
