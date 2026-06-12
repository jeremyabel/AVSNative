#pragma once

#include "engine/Effect.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

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

class Text : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::string TextString = "Hello;World";   // ';'-separated words
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

    static constexpr const char* kText          = "text";
    static constexpr const char* kFontFamily    = "fontFamily";
    static constexpr const char* kFontSize      = "fontSize";
    static constexpr const char* kBold          = "bold";
    static constexpr const char* kItalic        = "italic";
    static constexpr const char* kColor         = "color";
    static constexpr const char* kOutlineColor  = "outlineColor";
    static constexpr const char* kOutline       = "outline";
    static constexpr const char* kLegacyOutline = "legacyOutline";
    static constexpr const char* kShadow        = "shadow";
    static constexpr const char* kOutlineSize   = "outlineSize";
    static constexpr const char* kBlend         = "blendMode";
    static constexpr const char* kHAlign        = "halign";
    static constexpr const char* kVAlign        = "valign";
    static constexpr const char* kXShift        = "xshift";
    static constexpr const char* kYShift        = "yshift";
    static constexpr const char* kRandomPos     = "randomPos";
    static constexpr const char* kOnBeat        = "onbeat";
    static constexpr const char* kNormSpeed     = "normSpeed";
    static constexpr const char* kOnBeatSpeed   = "onbeatSpeed";
    static constexpr const char* kInsertBlank   = "insertBlank";
    static constexpr const char* kRandomWord    = "randomWord";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Text"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

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
