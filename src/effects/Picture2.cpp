#include "effects/Picture2.h"
#include "engine/FBOManager.h"

#include <bgfx/bgfx.h>
#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_picture2.sc.bin.h"

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

static const char* kBlendNames[] = {
    "Replace", "Additive", "Maximum", "50/50",
    "Subtractive 1", "Subtractive 2", "Multiply",
    "Adjustable", "XOR", "Minimum", "Ignore",
};

const std::vector<Field>& Picture2::Fields() const
{
    static const std::vector<Field> kFields = {
        SelectI(&Picture2Config::BlendMode, "blendMode", "Blend Mode",
                { kBlendNames[0], kBlendNames[1], kBlendNames[2], kBlendNames[3],
                  kBlendNames[4], kBlendNames[5], kBlendNames[6], kBlendNames[7],
                  kBlendNames[8], kBlendNames[9], kBlendNames[10] }),
        RangeI(&Picture2Config::AdjustBlend, "adjustBlend", "Blend Amount", 0, 255),
        ::Bool(&Picture2Config::Bilinear, "bilinear", "Bilinear"),
        SelectI(&Picture2Config::OnBeatBlendMode, "onBeatBlendMode", "On-Beat Blend Mode",
                { kBlendNames[0], kBlendNames[1], kBlendNames[2], kBlendNames[3],
                  kBlendNames[4], kBlendNames[5], kBlendNames[6], kBlendNames[7],
                  kBlendNames[8], kBlendNames[9], kBlendNames[10] }),
        RangeI(&Picture2Config::OnBeatAdjustBlend, "onBeatAdjustBlend", "On-Beat Blend Amount", 0, 255),
        ::Bool(&Picture2Config::OnBeatBilinear, "onBeatBilinear", "On-Beat Bilinear"),
    };
    return kFields;
}

// ── GetConfig / SetConfig ─────────────────────────────────────────────────────

nlohmann::json Picture2::GetConfig() const
{
    nlohmann::json j = ReflectedEffect<Picture2Config>::GetConfig();
    j["imageData"] = Cfg.ImageData;
    return j;
}

void Picture2::SetConfig(const nlohmann::json& cfg)
{
    ReflectedEffect<Picture2Config>::SetConfig(cfg);

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

void Picture2::OnConfigChanged(const std::vector<std::string>& /*changed*/) {}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Picture2::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv,  sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_picture2_spv, sizeof(fs_picture2_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_inputUnif  = bgfx::createUniform("s_input",    bgfx::UniformType::Sampler);
    m_imageUnif  = bgfx::createUniform("s_image",    bgfx::UniformType::Sampler);
    m_paramsUnif = bgfx::createUniform("u_p2Params", bgfx::UniformType::Vec4);

    m_inited = true;

    if (!Cfg.ImageData.empty())
        LoadImage(Cfg.ImageData);
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

// ── LoadImage ─────────────────────────────────────────────────────────────────

void Picture2::LoadImage(const std::string& dataUrl)
{
    if (bgfx::isValid(m_imageTex))
    {
        bgfx::destroy(m_imageTex);
        m_imageTex = BGFX_INVALID_HANDLE;
    }
    m_imgW = m_imgH = 0;

    if (dataUrl.empty()) return;

    const auto commaPos = dataUrl.find(',');
    if (commaPos == std::string::npos) return;

    const std::vector<uint8_t> raw = Base64Decode(dataUrl.substr(commaPos + 1));
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

    const int  blend  = Ctx.IsBeat() ? Cfg.OnBeatBlendMode   : Cfg.BlendMode;
    const bool linear = Ctx.IsBeat() ? Cfg.OnBeatBilinear     : Cfg.Bilinear;
    const int  adjust = Ctx.IsBeat() ? Cfg.OnBeatAdjustBlend  : Cfg.AdjustBlend;

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
