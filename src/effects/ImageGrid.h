#pragma once

#include "engine/Reflect.h"
#include "engine/LuaRuntime.h"

#include <bgfx/bgfx.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

struct GifStream;  // streaming GIF decoder (src/thirdparty/StbGifStream.h)

// Image Grid: tiles a provided image across the viewport as a repeating grid, with a
// per-frame Lua-scripted transform. Init/Frame/Beat code blocks set the built-in vars
// x,y (translate, in tile units), sizex,sizey (tile scale/zoom), r (rotation, radians);
// width,height (viewport) and b (beat) are engine-set each frame. Tiles carry the image
// aspect ratio so the image isn't stretched. Single full-screen GPU pass — Lua drives
// the shader uniforms. See fs_imagegrid.sc.
struct ImageGridConfig
{
    std::string ImageData  = "";  // slot 0 bundle asset ref/name; empty = built-in checkerboard
    std::string ImageData2 = "";  // slot 1 bundle asset ref/name (for A/B toggle testing)
    int         ActiveImage = 0;  // which slot is displayed (0 or 1)
    std::string InitCode  = "x=0; y=0; sizex=0.25; sizey=0.25; r=0;";
    std::string FrameCode = "";
    std::string BeatCode  = "";
    int         BlendMode = 0;    // 0=Replace, 1=Additive, 2=50/50, 3=Alpha
};

class ImageGrid : public ReflectedEffect<ImageGridConfig>
{
public:
    void Init() override;
    void Destroy()                            override;
    void Render(const RenderContext& Context) override;

    nlohmann::json GetConfig() const override;
    void           SetConfig(const nlohmann::json& cfg) override;

    std::vector<PresetAsset> CollectAssets() const override;
    void ApplyAsset(const std::string& key, const std::string& name,
                    std::vector<uint8_t> bytes) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

    int GetImageW()    const { return m_imgW; }
    int GetImageH()    const { return m_imgH; }
    int GetFrameCount() const { return (int)m_frames.size(); }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> kFields = {
            SelectI(&ImageGridConfig::BlendMode, "blendMode", "Blend",
                    { "Replace", "Additive", "50/50", "Alpha" }),
            Lua(&ImageGridConfig::InitCode,  "initCode",  "Init"),
            Lua(&ImageGridConfig::FrameCode, "frameCode", "Frame"),
            Lua(&ImageGridConfig::BeatCode,  "beatCode",  "Beat"),
        };
        return kFields;
    }
    std::string EffectName() const override { return "Image Grid"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void LoadActiveImage();            // (re)build the texture from the active slot's raw bytes
    void MakeDefaultImage();
    void ResetAnimation();
    void AdvanceAnimation();   // advance m_curFrame by wall-clock time, upload to m_imageTex
    void UploadCurrentFrame(); // upload m_frames[m_curFrame] to m_imageTex (if changed)
    void RescanAndSeed();
    void CompileAll();
    void RunInit();

    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif  = BGFX_INVALID_HANDLE;   // s_input
    bgfx::UniformHandle m_imageUnif  = BGFX_INVALID_HANDLE;   // s_image
    bgfx::UniformHandle m_xformUnif  = BGFX_INVALID_HANDLE;   // u_gridXform
    bgfx::UniformHandle m_paramsUnif = BGFX_INVALID_HANDLE;   // u_gridParams
    bgfx::TextureHandle m_imageTex   = BGFX_INVALID_HANDLE;  // single texture (mutable for GIFs)
    int m_imgW = 0, m_imgH = 0;

    // Animated-GIF playback (lazy decode + cache). Static images leave m_frames empty
    // and use an immutable m_imageTex. A GIF decodes one frame at a time on demand via
    // m_gif, caches each decoded frame's RGBA in m_frames, and uploads the current frame
    // into the single mutable m_imageTex. Once the whole GIF has been seen the decoder
    // is closed (m_fullyCached) and looping replays from the cache — no re-decoding.
    GifStream*                        m_gif = nullptr;   // open while frames remain to decode
    bool                              m_fullyCached = false;

    // Raw image bytes per slot (delivered via ApplyAsset from the preset bundle or the
    // UI file picker). Toggling/switching re-opens the GIF from these cached bytes — no
    // decode of the preset itself on the hot path. m_slotName keeps the original
    // filename so the asset re-bundles under the same name.
    std::vector<uint8_t> m_slotRaw[2];
    std::string          m_slotName[2];
    std::vector<std::vector<uint8_t>> m_frames;          // cached RGBA per decoded frame
    std::vector<int>                  m_frameDelaysMs;   // per-frame delay (ms)
    size_t m_curFrame      = 0;
    size_t m_uploadedFrame = (size_t)-1;  // which frame currently lives in m_imageTex
    double m_frameAccumMs  = 0.0;
    std::chrono::steady_clock::time_point m_lastTick{};
    bool   m_haveTick      = false;

    LuaRuntime m_lua;
    int  m_initRef  = -1;  // LUA_NOREF = -1
    int  m_frameRef = -1;
    int  m_beatRef  = -1;
    bool m_inited   = false;

    static const std::vector<std::string> k_builtins;
};
