#pragma once

#include "engine/Reflect.h"
#include "engine/LuaRuntime.h"

#include <string>

struct BumpConfig
{
    int  Depth          = 30;
    bool OnBeat         = false;
    int  OnBeatDuration = 15;
    int  OnBeatDepth    = 100;
    int  BlendMode      = 0;    // 0=Replace 1=Additive 2=50/50
    bool ShowLightPos   = false;
    bool InvertDepth    = false;
    std::string InitCode  = "t=0";
    std::string FrameCode = "x=0.5+cos(t)*0.3\ny=0.5+sin(t)*0.3\nt=t+0.1";
    std::string BeatCode  = "";
};

class Bump : public ReflectedEffect<BumpConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            RangeI (&BumpConfig::Depth,          "depth",          "Depth",            1, 100),
            Bool   (&BumpConfig::OnBeat,          "onBeat",         "On Beat"),
            RangeI (&BumpConfig::OnBeatDuration,  "onBeatDuration", "On Beat Duration", 0, 100),
            RangeI (&BumpConfig::OnBeatDepth,     "onBeatDepth",    "On Beat Depth",    1, 100),
            SelectI(&BumpConfig::BlendMode,       "blendMode",      "Blend Mode",
                    { "Replace", "Additive", "50/50" }),
            Bool   (&BumpConfig::ShowLightPos,    "showLightPos",   "Show Light Pos"),
            Bool   (&BumpConfig::InvertDepth,     "invertDepth",    "Invert Depth"),
            Lua    (&BumpConfig::InitCode,        "initCode",       "Init"),
            Lua    (&BumpConfig::FrameCode,       "frameCode",      "Frame"),
            Lua    (&BumpConfig::BeatCode,        "beatCode",       "Beat"),
        };
        return f;
    }
    std::string EffectName() const override { return "Bump"; }

    void OnConfigChanged(const std::vector<std::string>& Changed) override;

private:
    void SeedUserVars();
    void Recompile();

    bgfx::ProgramHandle Program    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle InputUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexelUnif  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUnif = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle FlagsUnif  = BGFX_INVALID_HANDLE;

    LuaRuntime m_lua;
    int  m_initRef  = -1;
    int  m_frameRef = -1;
    int  m_beatRef  = -1;
    bool m_inited   = false;

    int m_curDepth      = 30;
    int m_onBeatFadeout = 0;
};
