#pragma once

#include "engine/Effect.h"
#include "engine/ShaderCompiler.h"
#include "engine/LuaRuntime.h"
#include "engine/LuaUniformBridge.h"

#include <string>

// Dynamic Distance Modifier: a user GLSL "pixel" block remaps each pixel's radial
// distance `d` from center; Lua init/frame/beat blocks compute persistent variables
// (bridged into the GLSL via LuaUniformBridge). Pixel code may also call getspec/getosc
// (audio) directly. See ref/AVSWeb/src/effects/ddm.js.
class DynamicDistanceModifier : public Effect
{
public:
    
    static constexpr const char* NAME_InitCode = "initCode";
    static constexpr const char* NAME_BeatCode = "beatCode";
    static constexpr const char* NAME_FrameCode = "frameCode";
    static constexpr const char* NAME_PixelCode = "pixelCode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;
    
    std::string Name() const override { return "Dynamic Distance Modifier"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;
    
    std::string GetScriptError(const std::string& paramName) const override
    {
        if (paramName == NAME_PixelCode) 
        return ShaderError;
        
        return LuaContext.GetError(paramName);
    }
    
    void RecompileMain();
    void RecompileFrameCode();
    void RecompileBeatCode();

public:

    std::string PixelCode = "d = d * (1.0 + 0.08 * sin(t));";
    std::string InitCode = "t = 0.0;";
    std::string FrameCode = "t = t + 0.05;";
    std::string BeatCode = "";
    bool Blend = false;
    bool Bilinear = false;
    bool Compat = false;
    
private:
    
    void Recompile();
    std::string BuildFragGlsl() const;
    
    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params1Uniform = BGFX_INVALID_HANDLE; // (w, h, maxD, beat)
    bgfx::UniformHandle Params2Uniform = BGFX_INVALID_HANDLE; // (blend, 0, 0, 0)
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle AudioUniform = BGFX_INVALID_HANDLE;
    
    std::string ShaderError;

    LuaRuntime LuaContext;
    LuaUniformBridge LuaBridge;
    int LuaRefInit = -1;
    int LuaRefFrame = -1;
    int LuaRefBeat = -1;
    bool LuaInitComplete = false;
};
