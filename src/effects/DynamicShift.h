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
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::string InitCode  = "d = 0";
    std::string FrameCode = "x = sin(d) * 1.4\ny = 1.4 * cos(d)\nd = d + 0.01";
    std::string BeatCode  = "d = d + 2.0";
    bool Blend    = false;  // false = black border; true = alpha-blend with original
    bool Subpixel = true;   // linear filtering on the input texture
    bool Compat   = false;  // 8-bit integer bilinear matching win32 (needs Subpixel)

    static constexpr const char* kInitCode  = "initCode";
    static constexpr const char* kFrameCode = "frameCode";
    static constexpr const char* kBeatCode  = "beatCode";
    static constexpr const char* kBlend     = "blend";
    static constexpr const char* kSubpixel  = "subpixel";
    static constexpr const char* kCompat    = "bilinearCompat";

    void Init() override;
    void Destroy() override;
    void Render(const RenderContext& Context) override;

    std::string Name() const override { return "Dynamic Shift"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

    // Recompile entry points. Called after Deserialize and by the UI when the
    // matching code editor changes.
    void RecompileInitCode();   // rescans user vars, recompiles + reruns init
    void RecompileFrameCode();
    void RecompileBeatCode();

private:
    void RescanUserVars();

    bgfx::ProgramHandle Program     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle Params0Unif = BGFX_INVALID_HANDLE;  // (shiftX, shiftY, w, h)
    bgfx::UniformHandle Params1Unif = BGFX_INVALID_HANDLE;  // (blend, alpha, 0, 0)
    bgfx::UniformHandle InputUnif   = BGFX_INVALID_HANDLE;  // s_input

    LuaRuntime m_lua;
    int  m_initRef  = -1;
    int  m_frameRef = -1;
    int  m_beatRef  = -1;
    bool m_inited   = false;
};
