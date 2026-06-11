#pragma once

#include "engine/Reflect.h"
#include "engine/LuaRuntime.h"

#include <string>
#include <vector>

// Dynamic Shift: Lua init/frame/beat blocks compute x/y pixel-shift values each frame.
// The shader translates the input texture by (x,y) pixels; out-of-bounds areas become
// black (blend=false) or a weighted mix with the original (blend=true).
// See ref/AVSWeb/src/effects/dynamic-shift.js.
struct DynamicShiftConfig
{
    std::string InitCode  = "d = 0";
    std::string FrameCode = "x = sin(d) * 1.4\ny = 1.4 * cos(d)\nd = d + 0.01";
    std::string BeatCode  = "d = d + 2.0";
    bool Blend    = false;  // false = black border; true = alpha-blend with original
    bool Subpixel = true;   // linear filtering on the input texture
    bool Compat   = false;  // 8-bit integer bilinear matching win32 (needs Subpixel)
};

class DynamicShift : public ReflectedEffect<DynamicShiftConfig>
{
public:
    void Init() override;
    void Destroy()                               override;
    void Render(const RenderContext& Context)    override;

    void OnConfigChanged(const std::vector<std::string>& Changed) override;

    std::string GetScriptError(const std::string& paramName) const override
    {
        return m_lua.GetError(paramName);
    }

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Lua (&DynamicShiftConfig::InitCode,  "initCode",  "Init"),
            Lua (&DynamicShiftConfig::FrameCode, "frameCode", "Frame"),
            Lua (&DynamicShiftConfig::BeatCode,  "beatCode",  "Beat"),
            ::Bool(&DynamicShiftConfig::Blend,    "blend",    "Blend"),
            ::Bool(&DynamicShiftConfig::Subpixel, "subpixel", "Subpixel"),
            ::Bool(&DynamicShiftConfig::Compat, "bilinearCompat", "Bilinear (precise)"),
        };
        return f;
    }
    std::string EffectName() const override { return "Dynamic Shift"; }

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
