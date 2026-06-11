#pragma once

#include "engine/Reflect.h"
#include "engine/LuaRuntime.h"

#include <bgfx/bgfx.h>
#include <chrono>
#include <string>
#include <vector>

// Texer II: Lua-scripted particle stamping. Each frame, Lua point code runs n times;
// each particle sets (x,y,sizex,sizey,red,green,blue) to place a scaled, colorized copy
// of the loaded image. Particles accumulate into a CPU overlay buffer, which is uploaded
// to a texture and composited onto the input via the global line blend mode.
// See ref/AVSWeb/src/effects/texer2.js.
struct Texer2Config
{
    std::string ImageData  = "";         // bundle asset ref/name; empty = built-in soft-dot
    std::string InitCode   = "n=300";
    std::string FrameCode  = "";
    std::string BeatCode   = "";
    std::string PointCode  = "x=(i*2-1)*2;y=v;\nred=1-y*2;green=abs(y)*2;blue=y*2-1;";
    bool Resize   = false;   // scale sprites by sizex/sizey (bilinear)
    bool Wrap     = false;   // wrap-around stamping at buffer edges
    bool Colorize = true;    // multiply image by red/green/blue
};

class Texer2 : public ReflectedEffect<Texer2Config>
{
public:
    void Init() override;
    void Destroy()                               override;
    void Render(const RenderContext& Context)    override;

    nlohmann::json GetConfig() const override;
    void           SetConfig(const nlohmann::json& cfg) override;

    std::vector<PresetAsset> CollectAssets() const override;
    void ApplyAsset(const std::string& key, const std::string& name,
                    std::vector<uint8_t> bytes) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

    int GetImageW() const { return m_imgW; }
    int GetImageH() const { return m_imgH; }
    // Number of animation frames (>1 for an animated GIF, 0 for a static image).
    int GetFrameCount() const { return (int)m_frames.size(); }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> kFields = {
            ::Bool(&Texer2Config::Resize,    "resize",    "Resizing"),
            ::Bool(&Texer2Config::Wrap,      "wrap",      "Wrap Around"),
            ::Bool(&Texer2Config::Colorize,  "colorize",  "Color Filtering"),
            Lua  (&Texer2Config::InitCode,   "initCode",  "Init"),
            Lua  (&Texer2Config::FrameCode,  "frameCode", "Frame"),
            Lua  (&Texer2Config::BeatCode,   "beatCode",  "Beat"),
            Lua  (&Texer2Config::PointCode,  "pointCode", "Point"),
        };
        return kFields;
    }
    std::string EffectName() const override { return "Texer II"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void BuildFromRaw(const std::vector<uint8_t>& raw);
    void MakeDefaultImage();
    void ResetAnimation();
    void AdvanceAnimation();   // advance m_curFrame by wall-clock time, refresh m_imgPixels
    void EnsureOverlay(int w, int h);
    void RescanAndSeed();
    void CompileAll();
    void RunInit();
    void StampParticle(int cx, int cy, double sizex, double sizey,
                       float cr, float cg, float cb, int blendMode, float alpha,
                       int bufW, int bufH);

    bgfx::ProgramHandle m_prog        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputUnif   = BGFX_INVALID_HANDLE;   // s_input
    bgfx::UniformHandle m_overlayUnif = BGFX_INVALID_HANDLE;   // s_overlay
    bgfx::UniformHandle m_paramsUnif  = BGFX_INVALID_HANDLE;   // u_t2params
    bgfx::TextureHandle m_overlayTex  = BGFX_INVALID_HANDLE;

    std::vector<uint8_t> m_imgPixels;   // current frame, RGBA8 (what StampParticle() reads)
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

    std::vector<uint8_t> m_overlayBuf;
    int m_bufW = 0, m_bufH = 0;

    LuaRuntime m_lua;
    int  m_initRef  = -1;  // LUA_NOREF = -1
    int  m_frameRef = -1;
    int  m_beatRef  = -1;
    int  m_pointRef = -1;
    bool m_inited   = false;

    static const std::vector<std::string> k_builtins;
    static constexpr int k_maxN = 65536;
};
