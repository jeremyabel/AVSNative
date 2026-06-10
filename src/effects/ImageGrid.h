#pragma once

#include "engine/Reflect.h"
#include "engine/LuaRuntime.h"

#include <bgfx/bgfx.h>
#include <chrono>
#include <string>
#include <vector>

// Image Grid: tiles a provided image across the viewport as a repeating grid, with a
// per-frame Lua-scripted transform. Init/Frame/Beat code blocks set the built-in vars
// x,y (translate, in tile units), sizex,sizey (tile scale/zoom), r (rotation, radians);
// width,height (viewport) and b (beat) are engine-set each frame. Tiles carry the image
// aspect ratio so the image isn't stretched. Single full-screen GPU pass — Lua drives
// the shader uniforms. See fs_imagegrid.sc.
struct ImageGridConfig
{
    std::string ImageData = "";   // base64 data URL; empty = built-in checkerboard
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

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

    int GetImageW()    const { return m_imgW; }
    int GetImageH()    const { return m_imgH; }
    int GetFrameCount() const { return (int)m_gpuFrames.size(); }

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
    void LoadImage(const std::string& dataUrl);
    void MakeDefaultImage();
    void ResetAnimation();
    void AdvanceAnimation();  // advance m_curFrame by wall-clock time, upload to m_imageTex
    void RescanAndSeed();
    void CompileAll();
    void RunInit();

    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif  = BGFX_INVALID_HANDLE;   // s_input
    bgfx::UniformHandle m_imageUnif  = BGFX_INVALID_HANDLE;   // s_image
    bgfx::UniformHandle m_xformUnif  = BGFX_INVALID_HANDLE;   // u_gridXform
    bgfx::UniformHandle m_paramsUnif = BGFX_INVALID_HANDLE;   // u_gridParams
    bgfx::TextureHandle m_imageTex   = BGFX_INVALID_HANDLE;
    int m_imgW = 0, m_imgH = 0;

    // Animated-GIF playback. m_gpuFrames is empty for static images. For an animated GIF
    // each frame is a separate bgfx texture; AdvanceAnimation() just updates m_curFrame
    // and Render() reads m_gpuFrames[m_curFrame]. m_imageTex is the static-image texture
    // (BGFX_INVALID_HANDLE when m_gpuFrames is non-empty).
    std::vector<bgfx::TextureHandle> m_gpuFrames;      // one texture per GIF frame
    std::vector<int>                 m_frameDelaysMs;  // per-frame delay (ms)
    size_t m_curFrame      = 0;
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
