#pragma once

#include "engine/Effect.h"
#include "engine/ShaderCompiler.h"
#include "engine/LuaRuntime.h"
#include "engine/LuaUniformBridge.h"

#include <string>

class ColorModifier : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::string PixelCode =
        "// red, green, blue all start at the same channel intensity (0..1).\n"
        "// Modify them to remap that intensity to new R, G, B output values.\n"
        "// The stub runs once per channel: red output -> new R, green -> new G, blue -> new B.\n"
        "// beat = 1 on a beat. User vars declared in Init are also available.\n"
        "red   = red;\n"
        "green = green;\n"
        "blue  = blue;";
    std::string InitCode  = "";
    std::string FrameCode = "";
    std::string BeatCode  = "";

    static constexpr const char* kPixelCode = "pixelCode";
    static constexpr const char* kInitCode  = "initCode";
    static constexpr const char* kFrameCode = "frameCode";
    static constexpr const char* kBeatCode  = "beatCode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Color Modifier"; }
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
    void Recompile();      // builds GLSL → SPIRV → bgfx program

    std::string BuildFragGlsl() const;

    bgfx::ProgramHandle Program   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle InputUnif = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BeatUnif  = BGFX_INVALID_HANDLE;

    std::string m_shaderError;

    LuaRuntime       m_lua;
    LuaUniformBridge m_bridge;   // packs user Lua vars into u_cmod_v[N]
    int  m_initRef  = -1;
    int  m_frameRef = -1;
    int  m_beatRef  = -1;
    bool m_inited   = false;
};
