#pragma once

#include <bgfx/bgfx.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

class FBOManager;
struct VisData;

// Passed to every Effect::Render call.
// EffectChain sets InputTexture, OutputFBO, ViewId, and NextViewId before each call.
// Each effect reads from InputTexture, draws to ViewId (bound to OutputFBO), then
// calls FboManager->Swap(). Container effects (EffectList) self-allocate view IDs
// from NextViewId and run their inner chain before grabbing an output view.
struct RenderContext
{
    bgfx::TextureHandle     InputTexture = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle OutputFBO    = BGFX_INVALID_HANDLE;
    FBOManager*             FboManager   = nullptr;
    // Per-frame beat. Stored as a pointer (like LineBlendMode / NextViewId) so a
    // Custom BPM effect can rewrite it mid-chain and downstream effects in the same
    // frame see the change — and so it survives EffectList's context copy. Access it
    // through IsBeat() / SetBeat() rather than touching the pointer directly; those
    // are const, so a `const RenderContext&` can still rewrite the beat.
    bool*   IsBeatPtr = nullptr;
    int32_t Width  = 0, Height = 0, Frame = 0;
    double  Time   = 0.0;
    uint8_t ViewId = 0;
    // Shared frame-level view ID counter. Each effect grabs IDs from here and
    // advances by 4 (or more for multi-pass). Using a pointer so every effect in
    // the frame shares the same counter regardless of how the context is copied.
    uint8_t* NextViewId = nullptr;
    // Shared fullscreen-triangle vertex buffer. Created by Engine, never destroyed by effects.
    bgfx::VertexBufferHandle QuadVB = BGFX_INVALID_HANDLE;
    // 576×1 RGBA8 audio texture (full mip chain). Layout per texel:
    //   R = spec_L, G = spec_R, B = osc_L, A = osc_R  (raw 0–255 normalised to 0.0–1.0)
    // Use texelFetch(AudioTex, ivec2(bin, 0), lod) in dynamic shaders.
    // Matches AVS_Remake's AudioGLBuffer format and the AUDIO_GLSL_SRC snippet.
    bgfx::TextureHandle AudioTex = BGFX_INVALID_HANDLE;
    // CPU-side audio data (same frame as AudioTex). Null if no audio source is connected.
    const VisData* AudioData = nullptr;
    // Packed line-render state set by SetRenderMode, read by line-drawing effects (Simple, etc.).
    // Matches g_line_blend_mode packing: bits 16-23 = lineWidth (1-255), bits 8-15 = alpha,
    // bits 0-7 = blendMode (0-9). Pointer so downstream effects in the same frame share it.
    // Default (1u << 16) = lineWidth 1, alpha 0, Replace blend.
    uint32_t* LineBlendMode = nullptr;

    // Beat accessors. Both are const: they read/write the pointee, not the context,
    // so effects taking `const RenderContext&` can call SetBeat (Custom BPM does).
    bool IsBeat() const { return IsBeatPtr && *IsBeatPtr; }
    void SetBeat(bool b) const { if (IsBeatPtr) *IsBeatPtr = b; }
};

enum class ParamType
{
    Range,
    Color,
    Select,
    Bool,
    Number,
    Glsl,
    Lua,
    Colors,
};

struct ParamDesc
{
    std::string Name;
    std::string Label;
    ParamType Type = ParamType::Range;
    float Min = 0.0f;
    float Max = 1.0f;
    float Step = 0.01f;
    std::vector<std::string> Options; // Select only
};

struct EffectDesc
{
    std::string Name;
    std::vector<ParamDesc> Params;
};

class EffectChain;

// A binary asset (image/GIF) an effect contributes to / receives from a preset
// bundle. Key is the config key it belongs to (e.g. "imageData"); Name is the
// original file basename (e.g. "cat.gif"), used as the bundle entry name.
struct PresetAsset
{
    std::string          Key;
    std::string          Name;
    std::vector<uint8_t> Bytes;
};

class Effect
{
public:

    virtual void Init() = 0;
    virtual void Render(const RenderContext& Context) = 0;
    virtual EffectDesc GetDescriptor() const = 0;
    virtual nlohmann::json GetConfig() const = 0;
    virtual void SetConfig(const nlohmann::json& Config) = 0;
    virtual void Destroy() = 0;
    virtual ~Effect() = default;

    // Returns the inner EffectChain for container effects (e.g. EffectList).
    // Returns nullptr for all leaf effects.
    virtual EffectChain* GetInnerChain() { return nullptr; }

    // ── Preset bundle assets (images/GIFs stored as raw files in the .avsz) ────
    // Effects with binary assets override these. CollectAssets returns the raw
    // bytes to bundle on save (with the original filename). ApplyAsset delivers
    // raw bytes on load (called after SetConfig); the effect caches them, keeps
    // the name for re-save, and rebuilds. Default: no assets.
    virtual std::vector<PresetAsset> CollectAssets() const { return {}; }
    virtual void ApplyAsset(const std::string& /*key*/, const std::string& /*name*/,
                            std::vector<uint8_t> /*bytes*/) {}

    // Returns the last script error for a named param (Lua/Glsl blocks), or empty string.
    virtual std::string GetScriptError(const std::string& /*paramName*/) const { return {}; }

    // Total bgfx view IDs this effect consumes per frame. Used by EffectChain to
    // advance the counter even for disabled effects, keeping view IDs stable.
    // Default 4 covers ordinary leaf effects. Control-only effects (SetRenderMode,
    // Custom BPM) return 0 — EffectChain runs them in place with no view or FBO.
    // Multi-pass/container effects override with a larger count.
    virtual uint8_t ExpectedViewCount() const { return 4; }
};
