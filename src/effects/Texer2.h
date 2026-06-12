#pragma once

#include "engine/Effect.h"
#include "engine/KeyedImageList.h"
#include "engine/LuaRuntime.h"

#include <bgfx/bgfx.h>
#include <chrono>
#include <string>
#include <vector>

// Texer II: Lua-scripted particle stamping. Each frame, Lua point code runs n times;
// each particle sets (x,y,sizex,sizey,r,red,green,blue) to place a scaled, rotated,
// colorized copy of the loaded image (r = rotation in radians, default 0). Particles
// accumulate into a CPU overlay buffer, which is uploaded
// to a texture and composited onto the input via the global line blend mode.
// See ref/AVSWeb/src/effects/texer2.js.
class Texer2 : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int         Mode = 0;                // 0 = Single Image, 1 = Keyed Array
    std::string ImageData  = "";         // single-mode bundle asset ref/name; empty = built-in soft-dot
    std::string InitCode   = "n=300";
    std::string FrameCode  = "";
    std::string BeatCode   = "";
    std::string PointCode  = "x=(i*2-1)*2;y=v;\nred=1-y*2;green=abs(y)*2;blue=y*2-1;";
    bool Resize   = false;   // scale sprites by sizex/sizey (bilinear)
    bool Wrap     = false;   // wrap-around stamping at buffer edges
    bool Colorize = true;    // multiply image by red/green/blue

    // Keyed-array mode: a per-instance list of images, each bound to a keyboard key.
    KeyedImageList Keyed;

    static constexpr const char* kMode      = "mode";
    static constexpr const char* kImageData = "imageData";
    static constexpr const char* kInitCode  = "initCode";
    static constexpr const char* kFrameCode = "frameCode";
    static constexpr const char* kBeatCode  = "beatCode";
    static constexpr const char* kPointCode = "pointCode";
    static constexpr const char* kResize    = "resize";
    static constexpr const char* kWrap      = "wrap";
    static constexpr const char* kColorize  = "colorize";

    void Init() override;
    void Destroy() override;
    void Render(const RenderContext& Context) override;

    std::string Name() const override { return "Texer II"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::vector<PresetAsset> CollectAssets() const override;
    void ApplyAsset(const std::string& key, const std::string& name,
                    std::vector<uint8_t> bytes) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

    // (Re)builds the displayed image from the active source: the single-mode image in
    // Single mode, or the currently-selected keyed image in Keyed Array mode. Called
    // after an image loads and when the keyed selection changes (mapped key / UI).
    void LoadSelected();

    // Recompile entry points. Called after Deserialize and by the UI when the
    // matching code editor changes.
    void RecompileInitCode();   // rescans user vars, recompiles all blocks, reruns init
    void RecompileFrameCode();
    void RecompileBeatCode();
    void RecompilePointCode();

    int GetImageW() const { return m_imgW; }
    int GetImageH() const { return m_imgH; }
    // Number of animation frames (>1 for an animated GIF, 0 for a static image).
    int GetFrameCount() const { return (int)m_frames.size(); }

private:
    void BuildFromRaw(const std::vector<uint8_t>& raw);
    void MakeDefaultImage();
    void ResetAnimation();
    void AdvanceAnimation();   // advance m_curFrame by wall-clock time, refresh m_imgPixels
    void RescanAndSeed();
    void CompileAll();
    void RunInit();
    void EnsureImageTex();   // (re)create the GPU sprite texture when the image size changes

    // Seed pass: copy the input into the output FBO (vs_fullscreen + fs_blit).
    bgfx::ProgramHandle m_blitProg   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_blitTex    = BGFX_INVALID_HANDLE;   // s_texColor

    // Sprite pass: one quad per particle (vs_texer2_sprite + fs_texer2_sprite).
    bgfx::ProgramHandle m_spriteProg = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_spriteTexU = BGFX_INVALID_HANDLE;   // s_sprite
    bgfx::UniformHandle m_xformU     = BGFX_INVALID_HANDLE;   // u_xform (center.xy, half.zw)
    bgfx::UniformHandle m_rotU       = BGFX_INVALID_HANDLE;   // u_rot   (cos, sin, flipX, flipY)
    bgfx::UniformHandle m_colorU     = BGFX_INVALID_HANDLE;   // u_color
    bgfx::UniformHandle m_styleU     = BGFX_INVALID_HANDLE;   // u_style (style, param)
    bgfx::UniformHandle m_screenU    = BGFX_INVALID_HANDLE;   // u_screen (w, h)

    bgfx::VertexBufferHandle m_quadVB = BGFX_INVALID_HANDLE;  // static unit quad [-1,1]
    bgfx::TextureHandle      m_imageTex = BGFX_INVALID_HANDLE; // current sprite image, RGBA8
    int  m_texW = 0, m_texH = 0;     // size of m_imageTex
    bool m_spriteDirty = true;       // re-upload m_imgPixels to m_imageTex

    std::vector<uint8_t> m_imgPixels;   // current frame, RGBA8 (uploaded to m_imageTex)
    std::vector<uint8_t> m_raw;         // original image bytes (for re-bundling)
    std::string          m_name;        // original filename
    int m_imgW = 0, m_imgH = 0;

    // Animated-GIF playback. m_frames is empty for a static image (single decoded frame
    // lives only in m_imgPixels). For an animated GIF it holds every frame and m_imgPixels
    // is a copy of the currently displayed one, swapped in by AdvanceAnimation().
    std::vector<std::vector<uint8_t>> m_frames;        // all frames, RGBA8 (m_imgW*m_imgH*4 each)
    std::vector<int>                  m_frameDelaysMs; // per-frame delay (ms), parallel to m_frames
    size_t m_curFrame      = 0;
    double m_frameAccumMs  = 0.0;                      // time accumulated toward next frame
    std::chrono::steady_clock::time_point m_lastTick{};
    bool   m_haveTick      = false;

    LuaRuntime m_lua;
    int  m_initRef  = -1;  // LUA_NOREF = -1
    int  m_frameRef = -1;
    int  m_beatRef  = -1;
    int  m_pointRef = -1;
    bool m_inited   = false;

    static const std::vector<std::string> k_builtins;
    static constexpr int k_maxN = 65536;
};
