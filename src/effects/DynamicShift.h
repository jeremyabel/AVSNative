#pragma once

#include "engine/Effect.h"
#include "engine/LuaRuntime.h"

#include <string>

// Dynamic Shift: Lua init/frame/beat blocks compute x/y pixel-shift values each frame.
// The shader translates the input texture by (x,y) pixels; out-of-bounds areas become
// black (blend=false) or a weighted mix with the original (blend=true).
// See ref/AVSWeb/src/effects/dynamic-shift.js.
class DynamicShift : public Effect
{
public:

    static constexpr const char* NAME_InitCode = "initCode";
    static constexpr const char* NAME_FrameCode = "frameCode";
    static constexpr const char* NAME_BeatCode = "beatCode";

    void Init() override;
    void Destroy() override;
    void Render(const RenderContext& Context) override;

    std::string Name() const override { return "Dynamic Shift"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return LuaContext.GetError(paramName);
    }

    // Recompile entry points. Called after Deserialize and by the UI when the matching code editor changes.
    void RecompileInitCode();
    void RecompileFrameCode();
    void RecompileBeatCode();

public:

    std::string InitCode = "d = 0";
    std::string FrameCode = "x = sin(d) * 1.4\ny = 1.4 * cos(d)\nd = d + 0.01";
    std::string BeatCode = "d = d + 2.0";
    bool EnableBlend = false;  // false = black border; true = alpha-blend with original
    bool Bilinear = true;   // bilinear filtering on the input texture
    bool Compat = false;  // 8-bit integer bilinear matching win32 (needs Bilinear)

private:

    void RescanUserVars();

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params1Uniform = BGFX_INVALID_HANDLE;  // (shiftX, shiftY, w, h)
    bgfx::UniformHandle Params2Uniform = BGFX_INVALID_HANDLE;  // (blend, alpha, 0, 0)

    LuaRuntime LuaContext;
    int  LuaRefInit  = -1;
    int  LuaRefFrame = -1;
    int  m_beatRef  = -1;
    bool m_inited   = false;
};
