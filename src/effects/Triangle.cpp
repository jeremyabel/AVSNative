#include "Triangle.h"

#include "engine/JsonUtil.h"

#include "engine/AudioAnalyzer.h"
#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_simple.sc.bin.h"

#include <nanovg/nanovg.h>
#include <nanovg/nanovg_bgfx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// Built-in variable names ScanVarDecls must not treat as user-declared. Mirrors the
// JS reference's BUILTIN_VARS set.
static const std::vector<std::string> LuaBuiltIns = {
    "w", "h", "n", "i", "b", "skip",
    "x1", "y1", "red1", "green1", "blue1",
    "x2", "y2", "red2", "green2", "blue2",
    "x3", "y3", "red3", "green3", "blue3",
    "z1", "zbuf", "zbclear",
    "getspec", "getosc",
};

static const char* k_defaultInit     = "n = 1";
static const char* k_defaultFrame     = "";
static const char* k_defaultBeat      = "";
static const char* k_defaultTriangle =
    "x1 = -0.5; y1 = -0.5\n"
    "x2 =  0.5; y2 = -0.5\n"
    "x3 =  0.0; y3 =  0.5\n"
    "red1 = 1.0; green1 = 0.5; blue1 = 0.0";

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Triangle::Init()
{
    const bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_simple_spv,    sizeof(fs_simple_spv)));
    m_program        = bgfx::createProgram(VS, FS, true);
    m_inputSampler   = bgfx::createUniform("s_input",        bgfx::UniformType::Sampler);
    m_overlaySampler = bgfx::createUniform("s_overlay",      bgfx::UniformType::Sampler);
    m_paramsUniform  = bgfx::createUniform("u_simpleParams", bgfx::UniformType::Vec4);

    EnsureNvgContext();

    InitCode     = k_defaultInit;
    FrameCode    = k_defaultFrame;
    BeatCode     = k_defaultBeat;
    TriangleCode = k_defaultTriangle;

    m_outBuf.assign(kStride, 0.0f);

    SeedBuiltins();
    SeedUserVars();
    RecompileAll();
}

void Triangle::DestroyOverlay()
{
    if (m_overlayFbo)
    {
        nvgluDeleteFramebuffer(m_overlayFbo);
        m_overlayFbo = nullptr;
    }
    m_overlayW = m_overlayH = 0;
}

void Triangle::Destroy()
{
    DestroyOverlay();

    if (m_nvg)
    {
        nvgDelete(m_nvg);
        m_nvg = nullptr;
    }

    if (bgfx::isValid(m_paramsUniform))  bgfx::destroy(m_paramsUniform);
    if (bgfx::isValid(m_overlaySampler)) bgfx::destroy(m_overlaySampler);
    if (bgfx::isValid(m_inputSampler))   bgfx::destroy(m_inputSampler);
    if (bgfx::isValid(m_program))        bgfx::destroy(m_program);

    m_paramsUniform  = BGFX_INVALID_HANDLE;
    m_overlaySampler = BGFX_INVALID_HANDLE;
    m_inputSampler   = BGFX_INVALID_HANDLE;
    m_program        = BGFX_INVALID_HANDLE;
    // Lua refs are freed when m_lua destructs.
}

void Triangle::EnsureNvgContext()
{
    if (m_nvg && m_nvgEdgeAa == AntialiasingEnabled)
        return;

    // edgeaa is baked into the context at creation time, so recreate when it changes.
    DestroyOverlay();
    if (m_nvg)
        nvgDelete(m_nvg);

    m_nvgEdgeAa = AntialiasingEnabled;
    m_nvg = nvgCreate(AntialiasingEnabled ? 1 : 0, 0);
}

void Triangle::EnsureOverlay(int w, int h)
{
    if (m_overlayW == w && m_overlayH == h) return;
    DestroyOverlay();
    m_overlayFbo = nvgluCreateFramebuffer(m_nvg, w, h, 0);
    m_overlayW   = w;
    m_overlayH   = h;
}

// ── Script setup ──────────────────────────────────────────────────────────────

void Triangle::SeedBuiltins()
{
    for (const auto& v : LuaBuiltIns) m_lua.SeedVar(v);
    // n defaults to 1 so a freshly added effect draws something.
    m_lua.SetEnvNumber("n", 1);
}

void Triangle::SeedUserVars()
{
    const std::string allCode =
        InitCode + "\n" + FrameCode + "\n" + BeatCode + "\n" + TriangleCode;
    for (const auto& v : LuaRuntime::ScanVarDecls(allCode, LuaBuiltIns))
        m_lua.SeedVar(v);
}

// Per-frame defaults — matches the JS reference (C++ Triangle_Vars::init): vertices
// zeroed, colours reset to white. n, i, z1, zbuf, zbclear persist across frames.
void Triangle::ResetPerFrameVars()
{
    m_lua.SetEnvNumber("x1", 0.0); m_lua.SetEnvNumber("y1", 0.0);
    m_lua.SetEnvNumber("x2", 0.0); m_lua.SetEnvNumber("y2", 0.0);
    m_lua.SetEnvNumber("x3", 0.0); m_lua.SetEnvNumber("y3", 0.0);
    m_lua.SetEnvNumber("red1", 1.0); m_lua.SetEnvNumber("green1", 1.0); m_lua.SetEnvNumber("blue1", 1.0);
    m_lua.SetEnvNumber("red2", 1.0); m_lua.SetEnvNumber("green2", 1.0); m_lua.SetEnvNumber("blue2", 1.0);
    m_lua.SetEnvNumber("red3", 1.0); m_lua.SetEnvNumber("green3", 1.0); m_lua.SetEnvNumber("blue3", 1.0);
}

void Triangle::RecompileAll()
{
    m_lua.CompileBlock(InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(FrameCode, "frameCode", m_frameRef);
    m_lua.CompileBlock(BeatCode,  "beatCode",  m_beatRef);
    m_lua.CompileTriangleLoop(TriangleCode, m_triangleRef);
    SeedUserVars();
    m_lua.RunBlock(m_initRef, "initCode");
    m_inited = true;
}

nlohmann::json Triangle::Serialize() const
{
    return {
        { kInitCode,     InitCode     },
        { kFrameCode,    FrameCode    },
        { kBeatCode,     BeatCode     },
        { kTriangleCode, TriangleCode },
        { kAntialiasing, AntialiasingEnabled },
    };
}

void Triangle::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadString(j, kInitCode,     InitCode);
    JsonUtil::ReadString(j, kFrameCode,    FrameCode);
    JsonUtil::ReadString(j, kBeatCode,     BeatCode);
    JsonUtil::ReadString(j, kTriangleCode, TriangleCode);
    JsonUtil::ReadBool  (j, kAntialiasing, AntialiasingEnabled);

    RecompileAll();
}

// ── Render ────────────────────────────────────────────────────────────────────

void Triangle::Render(const RenderContext& Context)
{
    const int W = Context.Width;
    const int H = Context.Height;

    EnsureNvgContext();
    EnsureOverlay(W, H);
    if (!m_overlayFbo) return;

    m_lua.SetAudioData(Context.AudioData);

    // Per-frame scope setup.
    m_lua.SetEnvNumber("w", (double)W);
    m_lua.SetEnvNumber("h", (double)H);
    m_lua.SetEnvNumber("b", Context.IsBeat() ? 1.0 : 0.0);
    ResetPerFrameVars();

    if (!m_inited)
    {
        m_lua.RunBlock(m_initRef, "initCode");
        m_inited = true;
    }
    m_lua.RunBlock(m_frameRef, "frameCode");
    if (Context.IsBeat()) m_lua.RunBlock(m_beatRef, "beatCode");

    // Run the triangle loop into the output buffer.
    const int n = std::clamp((int)m_lua.GetEnvNumber("n"), 0, kMaxTris);
    const bool useZbuf = m_lua.GetEnvNumber("zbuf") != 0.0;

    // ── NanoVG overlay pass ─────────────────────────────────────────────────────
    nvgluSetViewFramebuffer(Context.ViewId, m_overlayFbo);
    bgfx::setViewClear(Context.ViewId, BGFX_CLEAR_COLOR, 0x00000000);
    bgfx::setViewRect(Context.ViewId, 0, 0, (uint16_t)W, (uint16_t)H);

    nvgluBindFramebuffer(m_overlayFbo);
    nvgBeginFrame(m_nvg, (float)W, (float)H, 1.0f);

    if (n > 0 && m_triangleRef != -1)
    {
        if ((int)m_outBuf.size() < n * kStride)
            m_outBuf.resize(n * kStride, 0.0f);

        m_lua.RunTriangleLoop(m_triangleRef, n, m_outBuf.data(), "triangleCode");

        // Build a draw order. Painter's algorithm (back→front by z) when zbuf is on —
        // an approximation of the original per-pixel depth buffer, exact for
        // non-intersecting triangles.
        std::vector<int> order;
        order.reserve(n);
        for (int t = 0; t < n; ++t)
            if (m_outBuf[t * kStride + 10] == 0.0f)  // skip == 0
                order.push_back(t);

        if (useZbuf)
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
                return m_outBuf[a * kStride + 9] < m_outBuf[b * kStride + 9];
            });

        // World coords [-1,+1] → screen. The reference's net on-screen result is
        // y=-1 = screen top, y=+1 = screen bottom (triangle.js compensates for an
        // extra canvas-upload Y-flip that nanovg does NOT have). nanovg's y=0 is
        // screen-top — same convention as Simple's raw overlay — so we map directly:
        //   px = (x+1) * 0.5 * W ;  py = (1 + y) * 0.5 * H   (y=-1 top, y=+1 bottom).
        const float hw = W * 0.5f;
        const float hh = H * 0.5f;

        for (int t : order)
        {
            const float* tri = m_outBuf.data() + t * kStride;
            const float x1 = tri[0], y1 = tri[1];
            const float x2 = tri[2], y2 = tri[3];
            const float x3 = tri[4], y3 = tri[5];

            if (!std::isfinite(x1) || !std::isfinite(y1) ||
                !std::isfinite(x2) || !std::isfinite(y2) ||
                !std::isfinite(x3) || !std::isfinite(y3))
                continue;

            const float cx1 = (x1 + 1.0f) * hw, cy1 = (1.0f + y1) * hh;
            const float cx2 = (x2 + 1.0f) * hw, cy2 = (1.0f + y2) * hh;
            const float cx3 = (x3 + 1.0f) * hw, cy3 = (1.0f + y3) * hh;

            const float r = std::clamp(tri[6], 0.0f, 1.0f);
            const float g = std::clamp(tri[7], 0.0f, 1.0f);
            const float b = std::clamp(tri[8], 0.0f, 1.0f);

            nvgBeginPath(m_nvg);
            nvgMoveTo(m_nvg, cx1, cy1);
            nvgLineTo(m_nvg, cx2, cy2);
            nvgLineTo(m_nvg, cx3, cy3);
            nvgClosePath(m_nvg);
            nvgFillColor(m_nvg, nvgRGBf(r, g, b));
            nvgFill(m_nvg);
        }
    }

    nvgEndFrame(m_nvg);
    nvgluBindFramebuffer(nullptr);

    // ── Composite pass: overlay → input (Replace where alpha > 0) ────────────────
    const uint8_t compView = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(compView, Context.OutputFBO);
    bgfx::setViewRect(compView, 0, 0, (uint16_t)W, (uint16_t)H);
    bgfx::setViewClear(compView, BGFX_CLEAR_NONE);

    const float params[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // Replace mode
    bgfx::setUniform(m_paramsUniform, params);
    bgfx::setTexture(0, m_inputSampler,   Context.InputTexture);
    bgfx::setTexture(1, m_overlaySampler, bgfx::getTexture(m_overlayFbo->handle));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(compView, m_program);

    Context.FboManager->Swap();
}

// ── Script error ──────────────────────────────────────────────────────────────

std::string Triangle::GetScriptError(const std::string& paramName) const
{
    return m_lua.GetError(paramName);
}
