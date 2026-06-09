#include "effects/Picture.h"
#include "engine/FBOManager.h"

#include <bgfx/bgfx.h>
#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_picture.sc.bin.h"

#include <cstring>
#include <vector>

// ── Base64 decode ─────────────────────────────────────────────────────────────

static std::vector<uint8_t> Base64Decode(const std::string& b64)
{
    static const char kChars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<uint8_t> out;
    out.reserve(b64.size() / 4 * 3 + 3);
    int accum = 0, bits = 0;
    for (unsigned char c : b64) {
        if (c == '=') break;
        const char* pos = std::strchr(kChars, (char)c);
        if (!pos) continue;
        accum = (accum << 6) | (int)(pos - kChars);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)(accum >> bits));
            accum &= (1 << bits) - 1;
        }
    }
    return out;
}

// ── Field table ───────────────────────────────────────────────────────────────

const std::vector<Field>& Picture::Fields() const
{
    static const std::vector<Field> kFields = {
        SelectI(&PictureConfig::BlendMode, "blendMode", "Blend Mode",
                { "Replace", "Additive", "50/50" }),
        ::Bool(&PictureConfig::OnBeatAdditive, "onBeatAdditive", "On-Beat Additive"),
        RangeI(&PictureConfig::OnBeatDuration, "onBeatDuration", "On-Beat Duration", 0, 32),
        SelectI(&PictureConfig::Fit, "fit", "Image Fit",
                { "Stretch", "Fit Width", "Fit Height" }),
    };
    return kFields;
}

// ── GetConfig / SetConfig ─────────────────────────────────────────────────────

nlohmann::json Picture::GetConfig() const
{
    nlohmann::json j = ReflectedEffect<PictureConfig>::GetConfig();
    j["imageData"] = Cfg.ImageData;
    return j;
}

void Picture::SetConfig(const nlohmann::json& cfg)
{
    ReflectedEffect<PictureConfig>::SetConfig(cfg);

    if (cfg.contains("imageData") && cfg["imageData"].is_string())
    {
        const std::string newData = cfg["imageData"].get<std::string>();
        if (newData != Cfg.ImageData)
        {
            Cfg.ImageData = newData;
            if (m_inited)
                LoadImage(Cfg.ImageData);
        }
    }
}

void Picture::OnConfigChanged(const std::vector<std::string>& /*changed*/) {}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Picture::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_picture_spv,    sizeof(fs_picture_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif  = bgfx::createUniform("s_input",     bgfx::UniformType::Sampler);
    m_imageUnif  = bgfx::createUniform("s_image",     bgfx::UniformType::Sampler);
    m_paramsUnif = bgfx::createUniform("u_picParams", bgfx::UniformType::Vec4);

    m_inited = true;

    if (!Cfg.ImageData.empty())
        LoadImage(Cfg.ImageData);
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

// ── LoadImage ─────────────────────────────────────────────────────────────────

void Picture::LoadImage(const std::string& dataUrl)
{
    if (bgfx::isValid(m_imageTex))
    {
        bgfx::destroy(m_imageTex);
        m_imageTex = BGFX_INVALID_HANDLE;
    }
    m_imgW = m_imgH = 0;

    if (dataUrl.empty()) return;

    // Parse "data:<mime>;base64,<data>"
    const auto commaPos = dataUrl.find(',');
    if (commaPos == std::string::npos) return;

    const std::vector<uint8_t> raw = Base64Decode(dataUrl.substr(commaPos + 1));
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

    if (Ctx.IsBeat && Cfg.OnBeatAdditive)
        m_cooldown = Cfg.OnBeatDuration;
    else if (m_cooldown > 0)
        --m_cooldown;

    const bool beatActive    = Ctx.IsBeat || m_cooldown > 0;
    const int  effectiveBlend =
        (Cfg.BlendMode == 1 || (Cfg.OnBeatAdditive && beatActive)) ? 1 : Cfg.BlendMode;

    const float imgAspect = (m_imgH > 0) ? (float)m_imgW / (float)m_imgH : 1.0f;
    const float scrAspect = (Ctx.Height > 0) ? (float)Ctx.Width / (float)Ctx.Height : 1.0f;

    float params[4] = {
        (float)effectiveBlend,
        (float)Cfg.Fit,
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
