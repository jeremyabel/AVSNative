#pragma once

#include "engine/Effect.h"
#include "engine/LuaRuntime.h"

#include <string>

class Bump : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int  Depth          = 30;   // 1–100
    bool OnBeat         = false;
    int  OnBeatDuration = 15;   // 0–100
    int  OnBeatDepth    = 100;  // 1–100
    int  BlendMode      = 0;    // 0=Replace 1=Additive 2=50/50
    bool ShowLightPos   = false;
    bool InvertDepth    = false;
    std::string InitCode  = "t=0";
    std::string FrameCode = "x=0.5+cos(t)*0.3\ny=0.5+sin(t)*0.3\nt=t+0.1";
    std::string BeatCode  = "";

    static constexpr const char* kDepth          = "depth";
    static constexpr const char* kOnBeat         = "onBeat";
    static constexpr const char* kOnBeatDuration = "onBeatDuration";
    static constexpr const char* kOnBeatDepth    = "onBeatDepth";
    static constexpr const char* kBlendMode      = "blendMode";
    static constexpr const char* kShowLightPos   = "showLightPos";
    static constexpr const char* kInvertDepth    = "invertDepth";
    static constexpr const char* kInitCode       = "initCode";
    static constexpr const char* kFrameCode      = "frameCode";
    static constexpr const char* kBeatCode       = "beatCode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Bump"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

    // Recompiles all Lua blocks, reseeds user vars, and reruns init. Called after
    // Deserialize and by the UI when any code editor changes.
    void RecompileCode();

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
