#pragma once

#include "engine/Reflect.h"
#include "engine/LuaRuntime.h"

#include <bgfx/bgfx.h>

#include <string>
#include <vector>

struct NVGcontext;
struct NVGLUframebuffer;

struct TriangleConfig
{
    std::string InitCode;
    std::string FrameCode;
    std::string BeatCode;
    std::string TriangleCode;
};

// Triangle — scriptable effect that draws filled triangles via NanoVG.
//
// Four Lua blocks (Init / Frame / Beat / Triangle) run like SuperScope's. The Triangle
// block runs `n` times per frame; each iteration produces one triangle from vertices
// x1,y1 / x2,y2 / x3,y3 and fill colour red1,green1,blue1 (vertices 2/3 colours are
// ignored, matching the original). When `zbuf` is non-zero, triangles are painter-sorted
// back-to-front by `z1` (an approximation of the original per-pixel depth buffer).
class Triangle : public ReflectedEffect<TriangleConfig>
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
            Lua(&TriangleConfig::InitCode,     "initCode",     "Init"),
            Lua(&TriangleConfig::FrameCode,    "frameCode",    "Frame"),
            Lua(&TriangleConfig::BeatCode,     "beatCode",     "Beat"),
            Lua(&TriangleConfig::TriangleCode, "triangleCode", "Triangle"),
        };
        return f;
    }
    std::string EffectName() const override { return "Triangle"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void SeedBuiltins();
    void SeedUserVars();
    void ResetPerFrameVars();
    void RecompileAll();

    LuaRuntime m_lua;
    int m_initRef     = -1;
    int m_frameRef    = -1;
    int m_beatRef     = -1;
    int m_triangleRef = -1;
    bool m_inited     = false;

    static constexpr int kStride    = 11;          // x1,y1,x2,y2,x3,y3,r,g,b,z,skip
    static constexpr int kMaxTris   = 64 * 1024;
    std::vector<float> m_outBuf;

    // NanoVG overlay (drawn each frame, then composited over the input).
    NVGcontext*       m_nvg        = nullptr;
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0;
    int               m_overlayH   = 0;
    void EnsureOverlay(int w, int h);
    void DestroyOverlay();

    // Composite pass (overlay → input), shared fs_simple shader (Replace mode).
    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
