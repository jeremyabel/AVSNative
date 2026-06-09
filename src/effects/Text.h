#pragma once

#include "engine/Reflect.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct NVGcontext;
struct NVGLUframebuffer;

// Text — renders one word at a time (semicolon-separated list) with NanoVG, cycling
// words over time or on beat, with alignment, shift, outline/shadow, and blend modes.
//
// NanoVG draws text from TrueType/OpenType fonts (fontstash/stb_truetype under the
// hood). Fonts are resolved from the requested family + bold/italic to a Windows system
// font file (C:\Windows\Fonts), with a fallback chain to Arial / Segoe UI.
//
// Differences from the original/AVSWeb worth noting: bold/italic come from the actual
// variant font file (no synthetic slanting); the "modern" stroke outline is approximated
// with the legacy 8-direction offset draw (NanoVG has no text stroking).

struct TextConfig
{
    std::string Text       = "Hello;World";   // ';'-separated words
    std::string FontFamily = "Arial";

    int FontSize = 32;     // 8..200
    bool Bold   = false;
    bool Italic = false;

    std::array<uint8_t, 3> Color        = { 255, 255, 255 };
    std::array<uint8_t, 3> OutlineColor = { 0, 0, 0 };
    bool Outline       = false;
    bool LegacyOutline = false;
    bool Shadow        = false;
    int  OutlineSize   = 2;   // 1..16

    int Blend  = 0;   // 0 = Replace, 1 = Additive, 2 = 50/50
    int HAlign = 1;   // 0 = Left, 1 = Center, 2 = Right
    int VAlign = 1;   // 0 = Top, 1 = Middle, 2 = Bottom
    int XShift = 0;   // 0..100 (%)
    int YShift = 0;   // 0..100 (%)
    bool RandomPos = false;

    bool OnBeat      = false;
    int  NormSpeed   = 15;   // frames per word (1..256)
    int  OnBeatSpeed = 15;   // visible frames after a beat (1..256)
    bool InsertBlank = false;
    bool RandomWord  = false;
};

class Text : public ReflectedEffect<TextConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    // text / fontFamily are strings, layered on top of the generic reflected config.
    nlohmann::json GetConfig() const override;
    void SetConfig(const nlohmann::json& Config) override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI(&TextConfig::FontSize, "fontSize", "Font Size", 8, 200),
            Bool(&TextConfig::Bold,   "bold",   "Bold"),
            Bool(&TextConfig::Italic, "italic", "Italic"),
            Color(&TextConfig::Color,        "color",        "Color"),
            Color(&TextConfig::OutlineColor, "outlineColor", "Outline/Shadow Color"),
            Bool(&TextConfig::Outline,       "outline",       "Outline"),
            Bool(&TextConfig::LegacyOutline, "legacyOutline", "Legacy Outline"),
            Bool(&TextConfig::Shadow,        "shadow",        "Shadow"),
            RangeI(&TextConfig::OutlineSize, "outlineSize", "Outline/Shadow Size", 1, 16),
            SelectI(&TextConfig::Blend,  "blendMode", "Blend Mode", { "Replace", "Additive", "50/50" }),
            SelectI(&TextConfig::HAlign, "halign", "Horizontal Align", { "Left", "Center", "Right" }),
            SelectI(&TextConfig::VAlign, "valign", "Vertical Align",    { "Top", "Middle", "Bottom" }),
            RangeI(&TextConfig::XShift, "xshift", "X Shift (%)", 0, 100),
            RangeI(&TextConfig::YShift, "yshift", "Y Shift (%)", 0, 100),
            Bool(&TextConfig::RandomPos, "randomPos", "Random Position"),
            Bool(&TextConfig::OnBeat,    "onbeat",    "On Beat"),
            RangeI(&TextConfig::NormSpeed,   "normSpeed",   "Frames Per Word", 1, 256),
            RangeI(&TextConfig::OnBeatSpeed, "onbeatSpeed", "Visible Frames",  1, 256),
            Bool(&TextConfig::InsertBlank, "insertBlank", "Insert Blank"),
            Bool(&TextConfig::RandomWord,  "randomWord",  "Random Word"),
        };
        return f;
    }
    std::string EffectName() const override { return "Text"; }

private:
    void EnsureOverlay(int w, int h);
    void DestroyOverlay();
    int  ResolveFontId(const std::string& family, bool bold, bool italic);
    int  GetFont(const std::string& path);

    // Word-cycling / position state (runtime, not serialized).
    int m_nf = 0, m_nb = 0, m_curWord = 0, m_oddEven = 0;
    int m_effHAlign = 1, m_effVAlign = 1;
    float m_effXShift = 0.0f, m_effYShift = 0.0f;

    // NanoVG.
    NVGcontext*       m_nvg        = nullptr;
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0, m_overlayH = 0;
    std::unordered_map<std::string, int> m_fontCache;   // path → nvg font id (-1 = failed)

    // Composite (fs_simple).
    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
