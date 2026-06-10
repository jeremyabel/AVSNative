#pragma once

#include "engine/Reflect.h"
#include "engine/ShaderCompiler.h"
#include "engine/LuaRuntime.h"
#include "engine/LuaUniformBridge.h"

#include <string>
#include <vector>

struct ColorModifierConfig
{
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
};

class ColorModifier : public ReflectedEffect<ColorModifierConfig>
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
            Glsl(&ColorModifierConfig::PixelCode, "pixelCode", "Pixel (GLSL)"),
            Lua (&ColorModifierConfig::InitCode,  "initCode",  "Init"),
            Lua (&ColorModifierConfig::FrameCode, "frameCode", "Frame"),
            Lua (&ColorModifierConfig::BeatCode,  "beatCode",  "Beat"),
        };
        return f;
    }
    std::string EffectName() const override { return "Color Modifier"; }

    void OnConfigChanged(const std::vector<std::string>& Changed) override;

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
