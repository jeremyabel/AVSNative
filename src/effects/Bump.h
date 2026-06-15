#pragma once

#include "engine/Effect.h"
#include "engine/LuaRuntime.h"

#include <string>

class Bump : public Effect
{
public:

    static constexpr const char* NAME_InitCode = "initCode";
    static constexpr const char* NAME_FrameCode = "frameCode";
    static constexpr const char* NAME_BeatCode = "beatCode";
    
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Bump"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return LuaContext.GetError(paramName);
    }

    void RecompileCode();

public:

    int Depth = 30;
    bool EnableOnBeatChange = false;
    int OnBeatDuration = 15;
    int OnBeatDepth = 100;
    int BlendMode = 0; // 0=Replace 1=Additive 2=50/50
    bool ShowLightPos = false;
    bool InvertDepth = false;
    std::string InitCode = "t=0";
    std::string FrameCode = "x=0.5+cos(t)*0.3\ny=0.5+sin(t)*0.3\nt=t+0.1";
    std::string BeatCode = "";

private:

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexelSizeUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params1Uniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params2Uniform = BGFX_INVALID_HANDLE;

    LuaRuntime LuaContext;
    int LuaRefInit = -1;
    int LuaRefFrame = -1;
    int LuaRefBeat = -1;
    bool LuaInitComplete = false;

    int CurrentDepth = 30;
    int OnBeatFadeout = 0;
};
