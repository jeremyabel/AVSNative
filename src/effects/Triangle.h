#pragma once

#include "engine/Effect.h"
#include "engine/LuaRuntime.h"

#include <bgfx/bgfx.h>

#include <string>
#include <vector>

struct NVGcontext;
struct NVGLUframebuffer;

// Triangle — scriptable effect that draws filled triangles via NanoVG.
//
// Four Lua blocks (Init / Frame / Beat / Triangle) run like SuperScope's. The Triangle
// block runs `n` times per frame; each iteration produces one triangle from vertices
// x1,y1 / x2,y2 / x3,y3 and fill colour red1,green1,blue1 (vertices 2/3 colours are
// ignored, matching the original). When `zbuf` is non-zero, triangles are painter-sorted
// back-to-front by `z1` (an approximation of the original per-pixel depth buffer).
class Triangle : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    std::string InitCode;
    std::string FrameCode;
    std::string BeatCode;
    std::string TriangleCode;
    bool        AntialiasingEnabled = true;

    static constexpr const char* kInitCode     = "initCode";
    static constexpr const char* kFrameCode    = "frameCode";
    static constexpr const char* kBeatCode     = "beatCode";
    static constexpr const char* kTriangleCode = "triangleCode";
    static constexpr const char* kAntialiasing = "antialiasing";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Triangle"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetScriptError(const std::string& paramName) const override;

    // Recompiles all four Lua blocks and reruns init. Called after Deserialize and
    // by the UI when any code editor changes.
    void RecompileAll();

private:
    void SeedBuiltins();
    void SeedUserVars();
    void ResetPerFrameVars();

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
    bool              m_nvgEdgeAa  = true; // tracks edgeaa used to create m_nvg
    NVGLUframebuffer* m_overlayFbo = nullptr;
    int               m_overlayW   = 0;
    int               m_overlayH   = 0;
    void EnsureNvgContext();
    void EnsureOverlay(int w, int h);
    void DestroyOverlay();

    // Composite pass (overlay → input), shared fs_simple shader (Replace mode).
    bgfx::ProgramHandle m_program        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_inputSampler   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_overlaySampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_paramsUniform  = BGFX_INVALID_HANDLE;
};
