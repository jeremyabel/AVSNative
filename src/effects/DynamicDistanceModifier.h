#pragma once

#include "engine/Reflect.h"
#include "engine/ShaderCompiler.h"
#include "engine/LuaRuntime.h"
#include "engine/LuaUniformBridge.h"

#include <string>
#include <vector>

// Dynamic Distance Modifier: a user GLSL "pixel" block remaps each pixel's radial
// distance `d` from center; Lua init/frame/beat blocks compute persistent variables
// (bridged into the GLSL via LuaUniformBridge). Pixel code may also call getspec/getosc
// (audio) directly. See ref/AVSWeb/src/effects/ddm.js.
struct DynamicDistanceModifierConfig
{
    std::string PixelCode =
        "// d = normalized distance from center (0..1, 1 = corner)\n"
        "// r = angle (radians), t = time (a user var from Init/Frame), b = beat (0/1)\n"
        "// Modify d to remap each radial ring. e.g. zoom: d = d * 0.9;\n"
        "d = d * (1.0 + 0.08 * sin(t));";
    std::string InitCode  = "t = 0.0;\nu = 1.0;";
    std::string FrameCode = "t = t + 0.05;";
    std::string BeatCode  = "";
    bool Blend    = false;   // 50/50 with the original
    bool Bilinear = false;   // linear vs nearest input sampling
};

class DynamicDistanceModifier : public ReflectedEffect<DynamicDistanceModifierConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        if (paramName == "pixelCode") return m_shaderError;
        return m_lua.GetError(paramName);
    }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            ::Bool(&DynamicDistanceModifierConfig::Blend,    "blend",     "Blend"),
            ::Bool(&DynamicDistanceModifierConfig::Bilinear, "bilinear",  "Bilinear Filtering"),
            Glsl(&DynamicDistanceModifierConfig::PixelCode,  "pixelCode", "Pixel (GLSL)"),
            Lua (&DynamicDistanceModifierConfig::InitCode,   "initCode",  "Init"),
            Lua (&DynamicDistanceModifierConfig::FrameCode,  "frameCode", "Frame"),
            Lua (&DynamicDistanceModifierConfig::BeatCode,   "beatCode",  "Beat"),
        };
        return f;
    }
    std::string EffectName() const override { return "Dynamic Distance Modifier"; }

    void OnConfigChanged(const std::vector<std::string>& Changed) override;

private:
    void Recompile();
    std::string BuildFragGlsl() const;

    bgfx::ProgramHandle Program    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params0Unif = BGFX_INVALID_HANDLE;  // (w, h, maxD, beat)
    bgfx::UniformHandle Params1Unif = BGFX_INVALID_HANDLE;  // (blend, 0, 0, 0)
    bgfx::UniformHandle InputUnif   = BGFX_INVALID_HANDLE;  // s_input
    bgfx::UniformHandle AudioUnif   = BGFX_INVALID_HANDLE;  // s_audio

    std::string m_shaderError;

    LuaRuntime       m_lua;
    LuaUniformBridge m_bridge;   // packs user Lua vars into u_ddm_v[N]
    int  m_initRef  = -1;
    int  m_frameRef = -1;
    int  m_beatRef  = -1;
    bool m_inited   = false;
};
