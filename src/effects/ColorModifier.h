#pragma once

#include "engine/Effect.h"
#include "engine/ShaderCompiler.h"
#include "engine/LuaRuntime.h"
#include "engine/LuaUniformBridge.h"

#include <string>

class ColorModifier : public Effect
{
public:

    static constexpr const char* NAME_PixelCode = "pixelCode";
    static constexpr const char* NAME_InitCode = "initCode";
    static constexpr const char* NAME_FrameCode = "frameCode";
    static constexpr const char* NAME_BeatCode = "beatCode";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Color Modifier"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        if (paramName == NAME_PixelCode) 
            return ShaderError;

        return LuaContext.GetError(paramName);
    }

    // Recompiles after PixelCode or InitCode changes: recompiles the init block,
    // rescans user-var uniforms, rebuilds the GLSL, and reruns init. Called after
    // Deserialize and by the UI.
    void RecompileMain();
    void RecompileFrameCode();
    void RecompileBeatCode();

public:

    std::string PixelCode = "red = red;\ngreen = green;\nblue = blue;";
    std::string InitCode = "";
    std::string FrameCode = "";
    std::string BeatCode = "";

private:

    void Recompile();

    std::string BuildFragGlsl() const;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle BeatUniform = BGFX_INVALID_HANDLE;

    std::string ShaderError;

    LuaRuntime LuaContext;
    LuaUniformBridge LuaBridge;
    int LuaRefInit = -1;
    int LuaRefFrame = -1;
    int LuaRefBeat = -1;
    bool LuaInitComplete = false;
};
