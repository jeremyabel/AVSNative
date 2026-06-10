#include "Text.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <nanovg/nanovg.h>
#include <nanovg/nanovg_bgfx.h>

#include <algorithm>
#include <cstdlib>
#include <cctype>

// ── Font resolution ───────────────────────────────────────────────────────────

namespace
{
struct FontEntry
{
    const char* family;
    const char* files[4];   // [regular, bold, italic, boldItalic]
};

// Common Windows fonts. idx = (bold?1:0) + (italic?2:0).
const FontEntry k_fonts[] = {
    { "arial",            { "arial.ttf",  "arialbd.ttf",  "ariali.ttf",   "arialbi.ttf"  } },
    { "times new roman",  { "times.ttf",  "timesbd.ttf",  "timesi.ttf",   "timesbi.ttf"  } },
    { "courier new",      { "cour.ttf",   "courbd.ttf",   "couri.ttf",    "courbi.ttf"   } },
    { "verdana",          { "verdana.ttf","verdanab.ttf", "verdanai.ttf", "verdanaz.ttf" } },
    { "georgia",          { "georgia.ttf","georgiab.ttf", "georgiai.ttf", "georgiaz.ttf" } },
    { "tahoma",           { "tahoma.ttf", "tahomabd.ttf", "tahoma.ttf",   "tahomabd.ttf" } },
    { "trebuchet ms",     { "trebuc.ttf", "trebucbd.ttf", "trebucit.ttf", "trebucbi.ttf" } },
    { "impact",           { "impact.ttf", "impact.ttf",   "impact.ttf",   "impact.ttf"   } },
    { "comic sans ms",    { "comic.ttf",  "comicbd.ttf",  "comic.ttf",    "comicbd.ttf"  } },
    { "segoe ui",         { "segoeui.ttf","segoeuib.ttf", "segoeuii.ttf", "segoeuiz.ttf" } },
};

std::string FontsDir()
{
    const char* win = std::getenv("WINDIR");
    return (win ? std::string(win) : std::string("C:\\Windows")) + "\\Fonts\\";
}

std::string ToLower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

const FontEntry* FindFamily(const std::string& familyLower)
{
    // Aliases for the generic CSS families the AVSWeb effect accepts.
    if (familyLower == "sans-serif") return &k_fonts[0];                 // arial
    if (familyLower == "serif")      return &k_fonts[1];                 // times new roman
    if (familyLower == "monospace")  return &k_fonts[2];                 // courier new
    for (const auto& e : k_fonts)
        if (familyLower == e.family) return &e;
    return nullptr;
}
} // namespace

int Text::GetFont(const std::string& path)
{
    auto it = m_fontCache.find(path);
    if (it != m_fontCache.end()) return it->second;
    const int id = nvgCreateFont(m_nvg, path.c_str(), path.c_str());
    m_fontCache[path] = id;
    return id;
}

int Text::ResolveFontId(const std::string& family, bool bold, bool italic)
{
    const std::string dir = FontsDir();
    const int idx = (bold ? 1 : 0) + (italic ? 2 : 0);

    std::vector<std::string> candidates;
    if (const FontEntry* e = FindFamily(ToLower(family)))
    {
        candidates.push_back(dir + e->files[idx]);
        candidates.push_back(dir + e->files[0]);   // regular variant of same family
    }
    else
    {
        // Unknown family: try "<family>.ttf" verbatim (lowercased, spaces stripped).
        std::string f = ToLower(family);
        f.erase(std::remove(f.begin(), f.end(), ' '), f.end());
        if (!f.empty()) candidates.push_back(dir + f + ".ttf");
    }
    candidates.push_back(dir + "arial.ttf");
    candidates.push_back(dir + "segoeui.ttf");

    for (const auto& c : candidates)
    {
        const int id = GetFont(c);
        if (id >= 0) return id;
    }
    return -1;
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Text::Init()
{
    const bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_simple_spv,    sizeof(fs_simple_spv)));
    m_program        = bgfx::createProgram(VS, FS, true);
    m_inputSampler   = bgfx::createUniform("s_input",        bgfx::UniformType::Sampler);
    m_overlaySampler = bgfx::createUniform("s_overlay",      bgfx::UniformType::Sampler);
    m_paramsUniform  = bgfx::createUniform("u_simpleParams", bgfx::UniformType::Vec4);

    m_nvg = nvgCreate(1, 0);
}

void Text::DestroyOverlay()
{
    if (m_overlayFbo) { nvgluDeleteFramebuffer(m_overlayFbo); m_overlayFbo = nullptr; }
    m_overlayW = m_overlayH = 0;
}

void Text::Destroy()
{
    DestroyOverlay();
    if (m_nvg) { nvgDelete(m_nvg); m_nvg = nullptr; }
    m_fontCache.clear();

    if (bgfx::isValid(m_paramsUniform))  bgfx::destroy(m_paramsUniform);
    if (bgfx::isValid(m_overlaySampler)) bgfx::destroy(m_overlaySampler);
    if (bgfx::isValid(m_inputSampler))   bgfx::destroy(m_inputSampler);
    if (bgfx::isValid(m_program))        bgfx::destroy(m_program);
    m_paramsUniform = m_overlaySampler = m_inputSampler = BGFX_INVALID_HANDLE;
    m_program = BGFX_INVALID_HANDLE;
}

void Text::EnsureOverlay(int w, int h)
{
    if (m_overlayW == w && m_overlayH == h) return;
    DestroyOverlay();
    m_overlayFbo = nvgluCreateFramebuffer(m_nvg, w, h, 0);
    m_overlayW = w;
    m_overlayH = h;
}

// ── Render ────────────────────────────────────────────────────────────────────

void Text::Render(const RenderContext& Context)
{
    const int w = Context.Width, h = Context.Height;
    EnsureOverlay(w, h);
    if (!m_overlayFbo) return;

    const bool isBeat = Context.IsBeat();

    // ── Word-cycling state machine (ported from the AVSWeb reference). ───────────
    std::vector<std::string> words;
    {
        const std::string& t = Cfg.Text;
        size_t start = 0;
        while (true)
        {
            size_t sep = t.find(';', start);
            words.push_back(t.substr(start, sep == std::string::npos ? std::string::npos : sep - start));
            if (sep == std::string::npos) break;
            start = sep + 1;
        }
    }
    const int numWords = (int)words.size();

    const bool shouldAdvance = (!Cfg.OnBeat && m_nf >= Cfg.NormSpeed)
                            || (Cfg.OnBeat && isBeat && m_nb == 0);

    if (shouldAdvance)
    {
        if (!(Cfg.InsertBlank && m_oddEven % 2 == 0))
        {
            m_curWord = Cfg.RandomWord ? (std::rand() % numWords)
                                       : (m_curWord + 1) % numWords;
        }
        m_oddEven = (m_oddEven + 1) % 2;
    }

    if (Cfg.OnBeat && isBeat && m_nb == 0)
        m_nb = Cfg.OnBeatSpeed;

    if (shouldAdvance)
    {
        m_nf = 0;
        if (Cfg.RandomPos)
        {
            m_effHAlign = 0;
            m_effVAlign = 0;
            // Measure the word width to keep it on-screen.
            const int fontId = ResolveFontId(Cfg.FontFamily, Cfg.Bold, Cfg.Italic);
            float tw = 0.0f;
            if (fontId >= 0)
            {
                nvgFontFaceId(m_nvg, fontId);
                nvgFontSize(m_nvg, (float)Cfg.FontSize);
                const std::string& word = words[m_curWord];
                float bounds[4] = { 0, 0, 0, 0 };
                nvgTextBounds(m_nvg, 0, 0, word.c_str(), nullptr, bounds);
                tw = bounds[2] - bounds[0];
            }
            const float th = (float)Cfg.FontSize;
            const float rx = (float)std::rand() / (float)RAND_MAX;
            const float ry = (float)std::rand() / (float)RAND_MAX;
            m_effXShift = (tw < w) ? rx * ((w - tw) / w * 100.0f) : 0.0f;
            m_effYShift = (th < h) ? ry * ((h - th) / h * 100.0f) : 0.0f;
        }
        else
        {
            m_effHAlign = Cfg.HAlign;
            m_effVAlign = Cfg.VAlign;
            m_effXShift = (float)Cfg.XShift;
            m_effYShift = (float)Cfg.YShift;
        }
    }

    const bool blank = (Cfg.InsertBlank && m_oddEven == 0);
    const std::string displayText = blank ? std::string() : words[m_curWord];
    const bool visible = !(Cfg.OnBeat && m_nb == 0);

    if (!Cfg.OnBeat) m_nf++;
    if (Cfg.OnBeat && m_nb > 0) m_nb--;

    if (!visible) return;   // pass-through (no swap)

    // ── Draw text into the NanoVG overlay. ───────────────────────────────────────
    nvgluSetViewFramebuffer(Context.ViewId, m_overlayFbo);
    bgfx::setViewClear(Context.ViewId, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)w, (uint16_t)h);

    nvgluBindFramebuffer(m_overlayFbo);
    nvgBeginFrame(m_nvg, (float)w, (float)h, 1.0f);

    const int fontId = ResolveFontId(Cfg.FontFamily, Cfg.Bold, Cfg.Italic);
    if (fontId >= 0 && !displayText.empty())
    {
        nvgFontFaceId(m_nvg, fontId);
        nvgFontSize(m_nvg, (float)Cfg.FontSize);

        const int hflag = m_effHAlign == 0 ? NVG_ALIGN_LEFT : m_effHAlign == 1 ? NVG_ALIGN_CENTER : NVG_ALIGN_RIGHT;
        const int vflag = m_effVAlign == 0 ? NVG_ALIGN_TOP  : m_effVAlign == 1 ? NVG_ALIGN_MIDDLE : NVG_ALIGN_BOTTOM;
        nvgTextAlign(m_nvg, hflag | vflag);

        const float dx = m_effXShift * w / 100.0f;
        const float dy = m_effYShift * h / 100.0f;
        const float ax = m_effHAlign == 0 ? dx : m_effHAlign == 1 ? w / 2.0f + dx : w + dx;
        const float ay = m_effVAlign == 0 ? dy : m_effVAlign == 1 ? h / 2.0f + dy : h + dy;

        const char* str = displayText.c_str();
        const NVGcolor mainCol = nvgRGB(Cfg.Color[0], Cfg.Color[1], Cfg.Color[2]);
        const NVGcolor outCol  = nvgRGB(Cfg.OutlineColor[0], Cfg.OutlineColor[1], Cfg.OutlineColor[2]);
        const float os = (float)Cfg.OutlineSize;

        if (Cfg.Outline)
        {
            // NanoVG has no text stroke; both outline styles use the legacy 8-offset fill.
            static const int off[8][2] = { {-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0} };
            nvgFillColor(m_nvg, outCol);
            for (auto& o : off)
                nvgText(m_nvg, ax + o[0] * os, ay + o[1] * os, str, nullptr);
        }
        else if (Cfg.Shadow)
        {
            nvgFillColor(m_nvg, outCol);
            nvgText(m_nvg, ax + os, ay + os, str, nullptr);
        }

        nvgFillColor(m_nvg, mainCol);
        nvgText(m_nvg, ax, ay, str, nullptr);
    }

    nvgEndFrame(m_nvg);
    nvgluBindFramebuffer(nullptr);

    // ── Composite overlay → input (blend then alpha-mix, via fs_simple). ─────────
    // Text blend {0 replace, 1 additive, 2 50/50} → fs_simple modes {0, 1, 3}.
    const float mode = (Cfg.Blend == 1) ? 1.0f : (Cfg.Blend == 2) ? 3.0f : 0.0f;

    const uint8_t compView = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(compView, Context.OutputFBO);
    bgfx::setViewRect(compView, 0, 0, (uint16_t)w, (uint16_t)h);
    bgfx::setViewClear(compView, BGFX_CLEAR_NONE);

    const float params[4] = { mode, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(m_paramsUniform, params);
    bgfx::setTexture(0, m_inputSampler,   Context.InputTexture);
    bgfx::setTexture(1, m_overlaySampler, bgfx::getTexture(m_overlayFbo->handle));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(compView, m_program);

    Context.FboManager->Swap();
}

// ── Serialization (text / fontFamily strings on top of the reflected config). ──

nlohmann::json Text::GetConfig() const
{
    nlohmann::json j = ReflectedEffect::GetConfig();
    j["text"]       = Cfg.Text;
    j["fontFamily"] = Cfg.FontFamily;
    return j;
}

void Text::SetConfig(const nlohmann::json& j)
{
    ReflectedEffect::SetConfig(j);
    if (j.contains("text")       && j["text"].is_string())       Cfg.Text       = j["text"].get<std::string>();
    if (j.contains("fontFamily") && j["fontFamily"].is_string()) Cfg.FontFamily = j["fontFamily"].get<std::string>();
}
