#pragma once

#include "engine/Reflect.h"
#include "engine/LuaRuntime.h"

#include <bgfx/bgfx.h>

#include <string>
#include <vector>

// Dynamic Movement — a per-pixel coordinate-warp effect.
//
// The Pixel block is GLSL inlined into a runtime-compiled shader. It can modify the
// polar coords d (distance) / r (angle), or the cartesian coords x / y, and the blend
// weight `alpha`. The Init / Frame / Beat blocks run on the CPU in Lua; any variable
// *declared in Init* is "bridged" — its per-frame value is uploaded as a uniform that
// the Pixel GLSL reads by name.
//
// Two evaluation paths (matching the original):
//   * Grid mode (default): the Pixel code runs once per vertex of a gridW × gridH mesh
//     in a VERTEX shader; the GPU interpolates the resulting source-UV across each
//     triangle. Cheaper and reproduces the original's faceting on coarse grids.
//   * Direct mode: the Pixel code runs once per fragment (exact, no interpolation).
//
// Built-ins for the Pixel block: d, r, x, y, w, h, b (beat 0/1), alpha,
// getspec()/getosc() (audio). See ref/AVSWeb/src/effects/dynamic-movement.js.

struct DynamicMovementConfig
{
    std::string PixelCode;
    std::string FrameCode;
    std::string BeatCode;
    std::string InitCode;

    bool RectCoords     = false;  // modify x/y instead of d/r
    bool Wrap           = false;
    bool Blend          = false;
    bool Bilinear       = false;
    bool BilinearCompat = false;  // exact 8-bit integer bilinear (matches original)
    bool NoMove         = false;
    bool ShowUV         = false;

    int  BufferN        = 0;      // 0 = current frame, 1..8 = scratch buffer

    bool UseGrid        = true;
    int  GridW          = 16;
    int  GridH          = 16;
};

class DynamicMovement : public ReflectedEffect<DynamicMovementConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string GetScriptError(const std::string& paramName) const override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Glsl(&DynamicMovementConfig::PixelCode, "pixelCode", "Pixel"),
            Lua (&DynamicMovementConfig::FrameCode, "frameCode", "Frame"),
            Lua (&DynamicMovementConfig::BeatCode,  "beatCode",  "Beat"),
            Lua (&DynamicMovementConfig::InitCode,  "initCode",  "Init"),
            Bool(&DynamicMovementConfig::RectCoords,     "rectCoords",     "Cartesian Coords"),
            Bool(&DynamicMovementConfig::Wrap,           "wrap",           "Wrap"),
            Bool(&DynamicMovementConfig::Blend,          "blend",          "Blend"),
            Bool(&DynamicMovementConfig::Bilinear,       "bilinear",       "Bilinear"),
            Bool(&DynamicMovementConfig::BilinearCompat, "bilinearCompat", "Bilinear (precise)"),
            Bool(&DynamicMovementConfig::NoMove,         "noMove",         "No Movement"),
            Bool(&DynamicMovementConfig::ShowUV,         "showUV",         "Show UV (debug)"),
            Bool(&DynamicMovementConfig::UseGrid,        "useGrid",        "Use Grid"),
            RangeI(&DynamicMovementConfig::GridW, "gridW", "Grid Width",  1, 256),
            RangeI(&DynamicMovementConfig::GridH, "gridH", "Grid Height", 1, 256),
            SelectI(&DynamicMovementConfig::BufferN, "bufferN", "Source Buffer",
                    { "Current", "Buffer 1", "Buffer 2", "Buffer 3", "Buffer 4",
                      "Buffer 5", "Buffer 6", "Buffer 7", "Buffer 8" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Dynamic Movement"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    // Max bridged variables = kDynVec4 * 4 (one float each, packed into vec4s).
    static constexpr int kDynVec4 = 16;
    static constexpr int kMaxDyn  = kDynVec4 * 4;

    // Shared GLSL fragment that emits the per-vertex/per-fragment transform body
    // (built-in locals + bridged + local var decls + the user pixel code).
    std::string BuildTransformBody() const;
    std::string BuildDirectFragGlsl() const;
    std::string BuildGridVertGlsl() const;
    std::string BuildGridFragGlsl() const;

    void CompileShaders();
    void RescanAndCompile();   // rescan bridged/local vars + rebuild shaders
    void RecompileLua();       // (re)compile init/frame/beat + reseed + run init

    std::string CompileError;
    bool        m_usesAudio = false;

    // Variable bookkeeping (rebuilt on pixel/init code change).
    std::vector<std::string> m_bridged;  // declared in Init → uploaded as uniforms
    std::vector<std::string> m_locals;   // assigned only in Pixel → shader locals

    // Lua side.
    LuaRuntime m_lua;
    int  m_initRef  = -1;
    int  m_frameRef = -1;
    int  m_beatRef  = -1;
    bool m_inited   = false;

    // bgfx programs.
    bgfx::ProgramHandle ProgramDirect = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle ProgramGrid   = BGFX_INVALID_HANDLE;

    // Direct-mode fragment uniforms.
    bgfx::UniformHandle Params0Unif = BGFX_INVALID_HANDLE;  // w, h, b, rectCoords
    bgfx::UniformHandle Params1Unif = BGFX_INVALID_HANDLE;  // wrap, blend, noMove, showUV
    bgfx::UniformHandle Params2Unif = BGFX_INVALID_HANDLE;  // sameBuffer, bilinearCompat
    // Grid-mode vertex uniforms.
    bgfx::UniformHandle VParams0Unif = BGFX_INVALID_HANDLE; // w, h, b, rectCoords
    bgfx::UniformHandle VParams1Unif = BGFX_INVALID_HANDLE; // wrap, gridW, gridH
    // Grid-mode fragment uniforms.
    bgfx::UniformHandle FParams0Unif = BGFX_INVALID_HANDLE; // wrap, blend, noMove, showUV
    bgfx::UniformHandle FParams1Unif = BGFX_INVALID_HANDLE; // sameBuffer, bilinearCompat
    // Shared.
    bgfx::UniformHandle DynVarsUnif = BGFX_INVALID_HANDLE;  // vec4[kDynVec4]
    bgfx::UniformHandle SourceUnif  = BGFX_INVALID_HANDLE;  // uSource
    bgfx::UniformHandle InputUnif   = BGFX_INVALID_HANDLE;  // uInput
    bgfx::UniformHandle AudioUnif   = BGFX_INVALID_HANDLE;  // s_audio
};
