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
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
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
    bool Compat   = false;   // 8-bit integer bilinear matching win32 (needs Bilinear)

    static constexpr const char* kBlend     = "blend";
    static constexpr const char* kBilinear  = "bilinear";
    static constexpr const char* kCompat    = "bilinearCompat";
    static constexpr const char* kPixelCode = "pixelCode";
    static constexpr const char* kInitCode  = "initCode";
    static constexpr const char* kFrameCode = "frameCode";
    static constexpr const char* kBeatCode  = "beatCode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Dynamic Distance Modifier"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        if (paramName == kPixelCode) return m_shaderError;
        return m_lua.GetError(paramName);
    }

    // Recompiles after PixelCode or InitCode changes: recompiles the init block,
    // rescans user-var uniforms, rebuilds the GLSL, and reruns init. Called after
    // Deserialize and by the UI.
    void RecompileMain();
    // Recompiles just the frame / beat Lua blocks.
    void RecompileFrameCode();
    void RecompileBeatCode();

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
