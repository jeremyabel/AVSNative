#include "Texer.h"

#include "engine/FBOManager.h"

#include <stb/stb_image.h>

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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

const std::vector<Field>& Texer::Fields() const
{
    static const std::vector<Field> kFields = {
        ::Bool(&TexerConfig::AddToInput,   "addToInput",   "Add to Input"),
        ::Bool(&TexerConfig::Colorize,     "colorize",     "Colorize"),
        RangeI(&TexerConfig::NumParticles, "numParticles", "Particles", 1, 1024),
    };
    return kFields;
}

// ── GetConfig / SetConfig ─────────────────────────────────────────────────────

nlohmann::json Texer::GetConfig() const
{
    nlohmann::json j = ReflectedEffect<TexerConfig>::GetConfig();
    j["imageData"] = Cfg.ImageData;
    return j;
}

void Texer::SetConfig(const nlohmann::json& cfg)
{
    ReflectedEffect<TexerConfig>::SetConfig(cfg);

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

void Texer::OnConfigChanged(const std::vector<std::string>& /*changed*/) {}

// ── Default soft-dot image (21×21, matches texer.js makeDefaultImage) ─────────

void Texer::MakeDefaultImage()
{
    const int sz = 21;
    m_imgW = m_imgH = sz;
    m_imgPixels.resize(sz * sz * 4);
    const float r = (sz - 1) / 2.0f;
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            const float dx = x - r, dy = y - r;
            const float d  = std::sqrt(dx * dx + dy * dy) / r;
            const float t  = std::max(0.0f, 1.0f - d);
            const uint8_t v = (uint8_t)(t * t * 255.0f + 0.5f);
            const int i = (y * sz + x) * 4;
            m_imgPixels[i] = m_imgPixels[i+1] = m_imgPixels[i+2] = v;
            m_imgPixels[i+3] = 255;
        }
    }
}

// ── LoadImage ─────────────────────────────────────────────────────────────────

void Texer::LoadImage(const std::string& dataUrl)
{
    if (dataUrl.empty())
    {
        MakeDefaultImage();
        return;
    }

    const auto commaPos = dataUrl.find(',');
    if (commaPos == std::string::npos) { MakeDefaultImage(); return; }

    const std::vector<uint8_t> raw = Base64Decode(dataUrl.substr(commaPos + 1));
    if (raw.empty()) { MakeDefaultImage(); return; }

    int w = 0, h = 0, ch = 0;
    uint8_t* pixels = stbi_load_from_memory(raw.data(), (int)raw.size(), &w, &h, &ch, 4);
    if (!pixels) { MakeDefaultImage(); return; }

    m_imgW = w;
    m_imgH = h;
    m_imgPixels.assign(pixels, pixels + w * h * 4);
    stbi_image_free(pixels);
}

// ── GPU buffer management ─────────────────────────────────────────────────────

void Texer::EnsureBuffers(int w, int h)
{
    if (m_bufW == w && m_bufH == h)
        return;

    if (bgfx::isValid(m_stagingTex)) { bgfx::destroy(m_stagingTex); m_stagingTex = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_outTex))     { bgfx::destroy(m_outTex);     m_outTex     = BGFX_INVALID_HANDLE; }

    m_bufW = w;
    m_bufH = h;
    m_readBuf.assign(w * h * 4, 0);
    m_outBuf.assign(w * h * 4, 0);

    // Staging texture: blit destination + CPU read-back. Same format as FBO (RGBA8).
    m_stagingTex = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);

    // Output texture: CPU uploads stamped buffer here each frame, then shader blits it.
    m_outTex = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    // Invalidate any in-flight readback — the old buffer was a different size.
    m_hasReadback = false;
}

// ── CPU stamp ─────────────────────────────────────────────────────────────────

void Texer::Stamp(int cx, int cy, uint8_t cr, uint8_t cg, uint8_t cb, int fbW, int fbH)
{
    const int iw = m_imgW, ih = m_imgH;
    const int iwHalf = iw >> 1, ihHalf = ih >> 1;
    const int iwOther = iw - iwHalf, ihOther = ih - ihHalf;

    const int fbStartX = cx - iwHalf, fbStartY = cy - ihHalf;
    const int fbEndX   = cx + iwOther, fbEndY   = cy + ihOther;

    const int imgStartX = std::max(0, -fbStartX);
    const int imgStartY = std::max(0, -fbStartY);
    const int imgEndX   = iw - std::max(0, fbEndX - fbW);
    const int imgEndY   = ih - std::max(0, fbEndY - fbH);
    const int fbx0 = std::max(0, fbStartX);
    const int fby0 = std::max(0, fbStartY);

    if (imgEndX <= imgStartX || imgEndY <= imgStartY) return;

    const bool colorize = Cfg.Colorize;

    for (int iy = imgStartY, fby = fby0; iy < imgEndY; iy++, fby++)
    {
        const int imgRow = iy * iw;
        const int outRow = fby * fbW;
        for (int ix = imgStartX, fbx = fbx0; ix < imgEndX; ix++, fbx++)
        {
            const int si = (imgRow + ix) * 4;
            int sr = m_imgPixels[si];
            int sg = m_imgPixels[si + 1];
            int sb = m_imgPixels[si + 2];
            if (colorize) {
                sr = (sr * cr) >> 8;
                sg = (sg * cg) >> 8;
                sb = (sb * cb) >> 8;
            }
            const int di = (outRow + fbx) * 4;
            m_outBuf[di]     = (uint8_t)std::min(255, (int)m_outBuf[di]     + sr);
            m_outBuf[di + 1] = (uint8_t)std::min(255, (int)m_outBuf[di + 1] + sg);
            m_outBuf[di + 2] = (uint8_t)std::min(255, (int)m_outBuf[di + 2] + sb);
            m_outBuf[di + 3] = 255;
        }
    }
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Texer::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_blit_spv,       sizeof(fs_blit_spv)));
    m_prog = bgfx::createProgram(VS, FS, true);

    m_texUnif = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    m_inited = true;
    LoadImage(Cfg.ImageData);
}

void Texer::Destroy()
{
    if (bgfx::isValid(m_stagingTex)) bgfx::destroy(m_stagingTex);
    if (bgfx::isValid(m_outTex))     bgfx::destroy(m_outTex);
    if (bgfx::isValid(m_texUnif))    bgfx::destroy(m_texUnif);
    if (bgfx::isValid(m_prog))       bgfx::destroy(m_prog);

    m_stagingTex = BGFX_INVALID_HANDLE;
    m_outTex     = BGFX_INVALID_HANDLE;
    m_texUnif    = BGFX_INVALID_HANDLE;
    m_prog       = BGFX_INVALID_HANDLE;
    m_bufW = m_bufH = 0;
    m_hasReadback = false;
    m_inited      = false;
}

// ── Render ────────────────────────────────────────────────────────────────────

void Texer::Render(const RenderContext& Context)
{
    const int w = Context.Width, h = Context.Height;
    EnsureBuffers(w, h);

    const uint8_t blitView = Context.ViewId;
    const uint8_t drawView = Context.ViewId + 1;

    // View blitView was set up by EffectChain with output FBO + clear. We're not
    // drawing to it — only blitting — so cancel the clear to avoid a wasted GPU op.
    bgfx::setViewClear(blitView, BGFX_CLEAR_NONE);

    // ── 1. Schedule GPU blit: input texture → staging (BLIT_DST | READ_BACK) ────
    bgfx::blit(blitView,
        m_stagingTex, 0, 0, 0, 0,
        Context.InputTexture, 0, 0, 0, 0,
        (uint16_t)w, (uint16_t)h);

    // Schedule CPU readback. Data arrives after bgfx::frame() — valid next Render().
    bgfx::readTexture(m_stagingTex, m_readBuf.data());

    // ── 2. CPU stamp using last frame's read-back data ─────────────────────────
    if (m_hasReadback)
    {
        if (Cfg.AddToInput)
            m_outBuf = m_readBuf;  // start from the input
        else
            std::fill(m_outBuf.begin(), m_outBuf.end(), (uint8_t)0);

        int p = 0;
        const int maxP = Cfg.NumParticles;
        bool done = false;

        // Scan top-to-bottom, left-to-right (Vulkan y=0=top matches screen top).
        for (int y = 0; y < h && !done; y++)
        {
            for (int x = 0; x < w && !done; x++)
            {
                const int i = (y * w + x) * 4;
                const uint8_t r = m_readBuf[i], g = m_readBuf[i+1], b = m_readBuf[i+2];
                if ((r | g | b) != 0)
                {
                    Stamp(x, y, r, g, b, w, h);
                    if (++p >= maxP)
                        done = true;
                }
            }
        }

        // Ensure all pixels have alpha=255 for correct downstream sampling.
        for (int i = 3; i < w * h * 4; i += 4)
            m_outBuf[i] = 255;
    }
    else
    {
        // First frame: no readback data yet; output black.
        std::fill(m_outBuf.begin(), m_outBuf.end(), (uint8_t)0);
        for (int i = 3; i < w * h * 4; i += 4)
            m_outBuf[i] = 255;
    }

    m_hasReadback = true;

    // ── 3. Upload CPU output buffer → GPU texture, draw to output FBO ─────────
    bgfx::updateTexture2D(m_outTex, 0, 0, 0, 0, (uint16_t)w, (uint16_t)h,
        bgfx::copy(m_outBuf.data(), (uint32_t)(w * h * 4)));

    bgfx::setViewFrameBuffer(drawView, Context.OutputFBO);
    bgfx::setViewRect(drawView, 0, 0, (uint16_t)w, (uint16_t)h);

    bgfx::setTexture(0, m_texUnif, m_outTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(drawView, m_prog);

    Context.FboManager->Swap();
}
