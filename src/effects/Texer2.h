#pragma once

#include "engine/Reflect.h"
#include "engine/LuaRuntime.h"

#include <bgfx/bgfx.h>
#include <string>
#include <vector>

// Texer II: Lua-scripted particle stamping. Each frame, Lua point code runs n times;
// each particle sets (x,y,sizex,sizey,red,green,blue) to place a scaled, colorized copy
// of the loaded image. Particles accumulate into a CPU overlay buffer, which is uploaded
// to a texture and composited onto the input via the global line blend mode.
// See ref/AVSWeb/src/effects/texer2.js.
struct Texer2Config
{
    std::string ImageData  = "";         // base64 data URL; empty = built-in soft-dot
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

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

    int GetImageW() const { return m_imgW; }
    int GetImageH() const { return m_imgH; }

protected:
    const std::vector<Field>& Fields() const override;
    std::string EffectName() const override { return "Texer II"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void LoadImage(const std::string& dataUrl);
    void MakeDefaultImage();
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

    std::vector<uint8_t> m_imgPixels;
    int m_imgW = 0, m_imgH = 0;

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
