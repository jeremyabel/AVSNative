#include "DynamicMovement.h"

#include "engine/JsonUtil.h"

#include "engine/FBOManager.h"
#include "engine/ShaderCompiler.h"
#include "engine/AudioGlsl.h"
#include "engine/ShaderSnippets.h"

#include <algorithm>
#include <regex>
#include <unordered_set>

// Built-in names that must never be treated as user-declared (bridged) variables.
static const std::vector<std::string> k_builtins = {
    "b", "d", "r", "x", "y", "w", "h", "alpha", "getspec", "getosc"
};

static const char* k_defaultPixel =
    "d = d * 0.95;\n"
    "r = r + 0.02;";

// Procedural fullscreen-triangle VS (direct mode): outputs v_texcoord0 with (0,0) at
// the top-left, matching the Vulkan/bgfx texture convention.
static const char* k_directVertGlsl = R"(
#version 450
layout(location = 0) out vec4 v_texcoord0;
void main() {
    vec2 uv = vec2(
        (gl_VertexIndex == 1) ? 2.0 : 0.0,
        (gl_VertexIndex == 2) ? 2.0 : 0.0);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    v_texcoord0 = vec4(uv.x, 1.0 - uv.y, 0.0, 0.0);
}
)";

// 8-bit integer bilinear — shared with the static .sc effects (and Movement).
// See src/engine/ShaderSnippets.h / src/shaders/bilinear_compat.sh.
static const char* k_bilinearCompat = avs::kBilinearCompatGlsl;

// Scan GLSL pixel code for assigned identifiers (plain `=` and compound `+=` etc.).
static std::vector<std::string> ScanAssigned(const std::string& code)
{
    std::vector<std::string> out;
    std::unordered_set<std::string> seen;
    std::regex pat(R"(\b([a-zA-Z_]\w*)\s*(?:[-+*/%&|^]?)=(?!=))");
    for (auto it = std::sregex_iterator(code.begin(), code.end(), pat);
         it != std::sregex_iterator(); ++it)
    {
        const std::string name = (*it)[1].str();
        if (seen.insert(name).second) out.push_back(name);
    }
    return out;
}

static bool Contains(const std::vector<std::string>& v, const std::string& s)
{
    return std::find(v.begin(), v.end(), s) != v.end();
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void DynamicMovement::Init()
{
    PixelCode = k_defaultPixel;

    Params0Unif  = bgfx::createUniform("u_params0",  bgfx::UniformType::Vec4);
    Params1Unif  = bgfx::createUniform("u_params1",  bgfx::UniformType::Vec4);
    Params2Unif  = bgfx::createUniform("u_params2",  bgfx::UniformType::Vec4);
    VParams0Unif = bgfx::createUniform("u_vparams0", bgfx::UniformType::Vec4);
    VParams1Unif = bgfx::createUniform("u_vparams1", bgfx::UniformType::Vec4);
    FParams0Unif = bgfx::createUniform("u_fparams0", bgfx::UniformType::Vec4);
    FParams1Unif = bgfx::createUniform("u_fparams1", bgfx::UniformType::Vec4);
    DynVarsUnif  = bgfx::createUniform("u_dynVars",  bgfx::UniformType::Vec4, kDynVec4);
    SourceUnif   = bgfx::createUniform("uSource",    bgfx::UniformType::Sampler);
    InputUnif    = bgfx::createUniform("uInput",     bgfx::UniformType::Sampler);
    AudioUnif    = bgfx::createUniform("s_audio",    bgfx::UniformType::Sampler);

    for (const auto& v : k_builtins) m_lua.SeedVar(v);
    m_lua.SetEnvNumber("b", 0.0);

    RescanAndCompile();
    RecompileLua();
}

void DynamicMovement::Destroy()
{
    const auto kill = [](bgfx::ProgramHandle& p) { if (bgfx::isValid(p)) bgfx::destroy(p); p = BGFX_INVALID_HANDLE; };
    const auto killU = [](bgfx::UniformHandle& u) { if (bgfx::isValid(u)) bgfx::destroy(u); u = BGFX_INVALID_HANDLE; };

    kill(ProgramDirect);
    kill(ProgramGrid);
    killU(Params0Unif);  killU(Params1Unif);  killU(Params2Unif);
    killU(VParams0Unif); killU(VParams1Unif);
    killU(FParams0Unif); killU(FParams1Unif);
    killU(DynVarsUnif);  killU(SourceUnif); killU(InputUnif); killU(AudioUnif);
    m_inited = false;
}

// ── Shader generation ─────────────────────────────────────────────────────────

// Emits the transform body. Assumes w, h, b, gu, gv, and bool _rect are already in
// scope (plus the const PI and, if bridged, the u_dynVars uniform). Leaves `src_uv`
// (top-left UV space) and `alpha` defined for the caller to use.
std::string DynamicMovement::BuildTransformBody() const
{
    std::string s = R"(
    float x = gu * 2.0 - 1.0;
    float y = gv * 2.0 - 1.0;
    float max_d = 0.5 * sqrt(w * w + h * h);
    float d = sqrt((x * w * 0.5) * (x * w * 0.5) + (y * h * 0.5) * (y * h * 0.5)) / max_d;
    float r = atan(y * h, x * w) + PI * 0.5;
    float alpha = 0.5;
)";
    for (size_t i = 0; i < m_bridged.size(); ++i)
    {
        const char* comp = (i % 4 == 0) ? ".x" : (i % 4 == 1) ? ".y" : (i % 4 == 2) ? ".z" : ".w";
        s += "    float " + m_bridged[i] + " = u_dynVars[" + std::to_string(i / 4) + "]" + comp + ";\n";
    }
    for (const auto& l : m_locals)
        s += "    float " + l + " = 0.0;\n";

    s += "\n    // --- user pixel code ---\n";
    s += PixelCode;
    s += R"(
    // --- end user pixel code ---

    vec2 src_uv;
    if (!_rect)
        src_uv = vec2(0.5 + sin(r) * d * max_d / w, 0.5 + cos(r) * d * max_d / h);
    else
        src_uv = vec2((x + 1.0) * 0.5, (1.0 - y) * 0.5);
    src_uv.y = 1.0 - src_uv.y;   // flip V for the top-left texture origin
)";
    return s;
}

std::string DynamicMovement::BuildDirectFragGlsl() const
{
    const int nBridged = (int)m_bridged.size();

    std::string s = R"(#version 450
layout(location = 0) in  vec4 v_texcoord0;
layout(location = 0) out vec4 bgfx_FragData0;

layout(std140, binding = 1) uniform _FragParams {
    vec4 u_params0;   // w, h, b, rectCoords
    vec4 u_params1;   // wrap, blend, noMove, showUV
    vec4 u_params2;   // sameBuffer, bilinearCompat, _, _
)";
    if (nBridged > 0)
        s += "    vec4 u_dynVars[" + std::to_string(kDynVec4) + "];\n";
    s += R"(};

layout(binding = 2) uniform sampler2D uSource;
layout(binding = 3) uniform sampler2D uInput;
)";
    if (m_usesAudio) { s += "layout(binding = 4) uniform sampler2D s_audio;\n"; s += kAudioGlslFns; }

    s += "\nconst float PI = 3.14159265358979;\n";
    s += k_bilinearCompat;
    s += R"(
void main() {
    vec2 vUv = v_texcoord0.xy;
    float w = u_params0.x;
    float h = u_params0.y;
    float b = u_params0.z;
    bool _rect       = u_params0.w > 0.5;
    bool _wrap       = u_params1.x > 0.5;
    bool _blend      = u_params1.y > 0.5;
    bool _noMove     = u_params1.z > 0.5;
    bool _showUV     = u_params1.w > 0.5;
    bool _sameBuffer = u_params2.x > 0.5;
    bool _biCompat   = u_params2.y > 0.5;

    float gu = vUv.x;
    float gv = vUv.y;
)";
    s += BuildTransformBody();
    s += R"(
    if (_showUV) { bgfx_FragData0 = vec4(src_uv, 0.0, 1.0); return; }

    vec4 orig = texture(uInput, vUv);
    if (_noMove) {
        vec3 src = _sameBuffer ? vec3(0.0) : texture(uSource, vUv).rgb;
        bgfx_FragData0 = vec4(mix(orig.rgb, src, clamp(alpha, 0.0, 1.0)), 1.0);
        return;
    }

    vec2 final_uv = _wrap ? fract(src_uv) : clamp(src_uv, 0.0, 1.0);
    vec3 sampled = _biCompat
        ? bilinearCompat(uSource, final_uv, textureSize(uSource, 0))
        : texture(uSource, final_uv).rgb;

    bgfx_FragData0 = _blend
        ? vec4(mix(orig.rgb, sampled, clamp(alpha, 0.0, 1.0)), 1.0)
        : vec4(sampled, 1.0);
}
)";
    return s;
}

std::string DynamicMovement::BuildGridVertGlsl() const
{
    const int nBridged = (int)m_bridged.size();

    std::string s = R"(#version 450
layout(std140, binding = 0) uniform _VertParams {
    vec4 u_vparams0;   // w, h, b, rectCoords
    vec4 u_vparams1;   // wrap, gridW, gridH, _
)";
    if (nBridged > 0)
        s += "    vec4 u_dynVars[" + std::to_string(kDynVec4) + "];\n";
    s += "};\n";
    if (m_usesAudio) { s += "layout(binding = 4) uniform sampler2D s_audio;\n"; s += kAudioGlslFns; }

    s += R"(
const float PI = 3.14159265358979;

layout(location = 0) out vec2  v_srcUV;
layout(location = 1) out float v_alpha;
layout(location = 2) out vec2  v_screenUV;

void main() {
    float w = u_vparams0.x;
    float h = u_vparams0.y;
    float b = u_vparams0.z;
    bool  _rect = u_vparams0.w > 0.5;
    bool  _wrap = u_vparams1.x > 0.5;
    int gw = int(u_vparams1.y + 0.5);
    int gh = int(u_vparams1.z + 0.5);

    // Procedural grid mesh: 2 triangles (6 verts) per cell, no vertex buffer.
    int vi   = gl_VertexIndex;
    int cell = vi / 6;
    int sub  = vi - cell * 6;
    int col  = cell % gw;
    int row  = cell / gw;
    float x0 = float(col)       / float(gw) * 2.0 - 1.0;
    float x1 = float(col + 1)   / float(gw) * 2.0 - 1.0;
    float y0 = 1.0 - float(row)     / float(gh) * 2.0;
    float y1 = 1.0 - float(row + 1) / float(gh) * 2.0;
    vec2 pos;
    if      (sub == 0) pos = vec2(x0, y0);
    else if (sub == 1) pos = vec2(x1, y0);
    else if (sub == 2) pos = vec2(x0, y1);
    else if (sub == 3) pos = vec2(x1, y0);
    else if (sub == 4) pos = vec2(x1, y1);
    else               pos = vec2(x0, y1);

    gl_Position = vec4(pos, 0.0, 1.0);

    float gu = (pos.x + 1.0) * 0.5;
    float gv = (1.0 - pos.y) * 0.5;   // 0 = top
)";
    s += BuildTransformBody();
    s += R"(
    v_srcUV    = _wrap ? src_uv : clamp(src_uv, 0.0, 1.0);
    v_alpha    = clamp(alpha, 0.0, 1.0);
    v_screenUV = vec2(gu, gv);
}
)";
    return s;
}

std::string DynamicMovement::BuildGridFragGlsl() const
{
    std::string s = R"(#version 450
layout(location = 0) in  vec2  v_srcUV;
layout(location = 1) in  float v_alpha;
layout(location = 2) in  vec2  v_screenUV;
layout(location = 0) out vec4  bgfx_FragData0;

layout(std140, binding = 1) uniform _FragParams {
    vec4 u_fparams0;   // wrap, blend, noMove, showUV
    vec4 u_fparams1;   // sameBuffer, bilinearCompat, _, _
};

layout(binding = 2) uniform sampler2D uSource;
layout(binding = 3) uniform sampler2D uInput;
)";
    s += k_bilinearCompat;
    s += R"(
void main() {
    bool _wrap       = u_fparams0.x > 0.5;
    bool _blend      = u_fparams0.y > 0.5;
    bool _noMove     = u_fparams0.z > 0.5;
    bool _showUV     = u_fparams0.w > 0.5;
    bool _sameBuffer = u_fparams1.x > 0.5;
    bool _biCompat   = u_fparams1.y > 0.5;

    if (_showUV) { bgfx_FragData0 = vec4(v_srcUV, 0.0, 1.0); return; }

    vec4 orig = texture(uInput, v_screenUV);
    if (_noMove) {
        vec3 src = _sameBuffer ? vec3(0.0) : texture(uSource, v_screenUV).rgb;
        bgfx_FragData0 = vec4(mix(orig.rgb, src, v_alpha), 1.0);
        return;
    }

    vec2 final_uv = _wrap ? fract(v_srcUV) : clamp(v_srcUV, 0.0, 1.0);
    vec3 sampled = _biCompat
        ? bilinearCompat(uSource, final_uv, textureSize(uSource, 0))
        : texture(uSource, final_uv).rgb;

    bgfx_FragData0 = _blend
        ? vec4(mix(orig.rgb, sampled, v_alpha), 1.0)
        : vec4(sampled, 1.0);
}
)";
    return s;
}

void DynamicMovement::CompileShaders()
{
    if (bgfx::isValid(ProgramDirect)) { bgfx::destroy(ProgramDirect); ProgramDirect = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(ProgramGrid))   { bgfx::destroy(ProgramGrid);   ProgramGrid   = BGFX_INVALID_HANDLE; }

    const int nBridged = (int)m_bridged.size();
    const uint16_t dynUboBytes = (uint16_t)(kDynVec4 * 16);

    // Shared fullscreen-triangle VS for direct mode (no uniforms).
    std::vector<uint32_t> directVert;
    std::string vErr;
    if (!ShaderCompiler::GlslToSpirv(k_directVertGlsl, false, directVert, vErr))
    {
        CompileError = vErr;
        return;
    }

    // ── Direct program ──────────────────────────────────────────────────────────
    {
        std::vector<uint32_t> frag;
        if (ShaderCompiler::GlslToSpirv(BuildDirectFragGlsl(), true, frag, CompileError))
        {
            std::vector<BgfxUniformDesc> u = {
                { "u_params0", 0x12, 1,  0, 1, 0, 0, 0 },
                { "u_params1", 0x12, 1, 16, 1, 0, 0, 0 },
                { "u_params2", 0x12, 1, 32, 1, 0, 0, 0 },
            };
            uint16_t ubo = 48;
            if (nBridged > 0) { u.push_back({ "u_dynVars", 0x12, (uint8_t)kDynVec4, 48, (uint16_t)kDynVec4, 0, 0, 0 }); ubo = (uint16_t)(48 + dynUboBytes); }
            u.push_back({ "uSource", 0x30, 0, 2, 0, 0, 0, kTexFmt2DSampler });
            u.push_back({ "uInput",  0x30, 0, 3, 0, 0, 0, kTexFmt2DSampler });
            if (m_usesAudio) u.push_back({ "s_audio", 0x30, 0, 4, 0, 0, 0, kTexFmt2DSampler });

            const bgfx::ShaderHandle fs = ShaderCompiler::WrapFragmentSpirv(frag, u.data(), (int)u.size(), ubo);
            const bgfx::ShaderHandle vs = ShaderCompiler::WrapVertexSpirv(directVert, nullptr, 0, 0);
            if (bgfx::isValid(fs) && bgfx::isValid(vs))
                ProgramDirect = bgfx::createProgram(vs, fs, true);
            else { if (bgfx::isValid(fs)) bgfx::destroy(fs); if (bgfx::isValid(vs)) bgfx::destroy(vs); }
        }
    }

    // ── Grid program ────────────────────────────────────────────────────────────
    {
        std::vector<uint32_t> gvert, gfrag;
        std::string gvErr, gfErr;
        const bool okV = ShaderCompiler::GlslToSpirv(BuildGridVertGlsl(), false, gvert, gvErr);
        const bool okF = ShaderCompiler::GlslToSpirv(BuildGridFragGlsl(), true,  gfrag, gfErr);
        if (okV && okF)
        {
            // Vertex UBO (binding 0): u_vparams0, u_vparams1, [u_dynVars]. Audio sampler
            // (if used) is a vertex-stage sampler (type 0x20 = no fragment bit).
            std::vector<BgfxUniformDesc> vu = {
                { "u_vparams0", 0x02, 1,  0, 1, 0, 0, 0 },
                { "u_vparams1", 0x02, 1, 16, 1, 0, 0, 0 },
            };
            uint16_t vubo = 32;
            if (nBridged > 0) { vu.push_back({ "u_dynVars", 0x02, (uint8_t)kDynVec4, 32, (uint16_t)kDynVec4, 0, 0, 0 }); vubo = (uint16_t)(32 + dynUboBytes); }
            if (m_usesAudio) vu.push_back({ "s_audio", 0x20, 0, 4, 0, 0, 0, kTexFmt2DSampler });

            std::vector<BgfxUniformDesc> fu = {
                { "u_fparams0", 0x12, 1,  0, 1, 0, 0, 0 },
                { "u_fparams1", 0x12, 1, 16, 1, 0, 0, 0 },
                { "uSource", 0x30, 0, 2, 0, 0, 0, kTexFmt2DSampler },
                { "uInput",  0x30, 0, 3, 0, 0, 0, kTexFmt2DSampler },
            };

            const bgfx::ShaderHandle vs = ShaderCompiler::WrapVertexSpirv(gvert, vu.data(), (int)vu.size(), vubo);
            const bgfx::ShaderHandle fs = ShaderCompiler::WrapFragmentSpirv(gfrag, fu.data(), (int)fu.size(), 32);
            if (bgfx::isValid(vs) && bgfx::isValid(fs))
                ProgramGrid = bgfx::createProgram(vs, fs, true);
            else { if (bgfx::isValid(vs)) bgfx::destroy(vs); if (bgfx::isValid(fs)) bgfx::destroy(fs); }
        }
        else
        {
            if (CompileError.empty()) CompileError = okV ? gfErr : gvErr;
        }
    }
}

void DynamicMovement::RescanAndCompile()
{
    m_bridged = LuaRuntime::ScanVarDecls(InitCode, k_builtins);
    if ((int)m_bridged.size() > kMaxDyn)
        m_bridged.resize(kMaxDyn);

    m_locals.clear();
    for (const auto& name : ScanAssigned(PixelCode))
        if (!Contains(k_builtins, name) && !Contains(m_bridged, name))
            m_locals.push_back(name);

    m_usesAudio = PixelCode.find("getspec") != std::string::npos ||
                  PixelCode.find("getosc")  != std::string::npos;

    CompileShaders();
}

// ── Lua management ────────────────────────────────────────────────────────────

void DynamicMovement::RecompileLua()
{
    m_lua.CompileBlock(InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(FrameCode, "frameCode", m_frameRef);
    m_lua.CompileBlock(BeatCode,  "beatCode",  m_beatRef);

    const std::string all = InitCode + "\n" + FrameCode + "\n" + BeatCode;
    for (const auto& v : LuaRuntime::ScanVarDecls(all, k_builtins))
        m_lua.SeedVar(v);

    m_lua.SetEnvNumber("b", 0.0);
    m_lua.RunBlock(m_initRef, "initCode");
    m_inited = true;
}

void DynamicMovement::ApplyPixelCodeChange()
{
    RescanAndCompile();
}

void DynamicMovement::ApplyInitCodeChange()
{
    RescanAndCompile();
    RecompileLua();
}

void DynamicMovement::ApplyFrameCodeChange()
{
    m_lua.CompileBlock(FrameCode, "frameCode", m_frameRef);
}

void DynamicMovement::ApplyBeatCodeChange()
{
    m_lua.CompileBlock(BeatCode, "beatCode", m_beatRef);
}

nlohmann::json DynamicMovement::Serialize() const
{
    return {
        { kPixelCode,      PixelCode      },
        { kFrameCode,      FrameCode      },
        { kBeatCode,       BeatCode       },
        { kInitCode,       InitCode       },
        { kRectCoords,     RectCoords     },
        { kWrap,           Wrap           },
        { kBlend,          Blend          },
        { kBilinear,       Bilinear       },
        { kBilinearCompat, BilinearCompat },
        { kNoMove,         NoMove         },
        { kShowUV,         ShowUV         },
        { kUseGrid,        UseGrid        },
        { kGridW,          GridW          },
        { kGridH,          GridH          },
        { kBufferN,        BufferN        },
    };
}

void DynamicMovement::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadString(j, kPixelCode,      PixelCode);
    JsonUtil::ReadString(j, kFrameCode,      FrameCode);
    JsonUtil::ReadString(j, kBeatCode,       BeatCode);
    JsonUtil::ReadString(j, kInitCode,       InitCode);
    JsonUtil::ReadBool  (j, kRectCoords,     RectCoords);
    JsonUtil::ReadBool  (j, kWrap,           Wrap);
    JsonUtil::ReadBool  (j, kBlend,          Blend);
    JsonUtil::ReadBool  (j, kBilinear,       Bilinear);
    JsonUtil::ReadBool  (j, kBilinearCompat, BilinearCompat);
    JsonUtil::ReadBool  (j, kNoMove,         NoMove);
    JsonUtil::ReadBool  (j, kShowUV,         ShowUV);
    JsonUtil::ReadBool  (j, kUseGrid,        UseGrid);
    JsonUtil::ReadInt   (j, kGridW,          GridW);
    JsonUtil::ReadInt   (j, kGridH,          GridH);
    JsonUtil::ReadInt   (j, kBufferN,        BufferN);

    ApplyInitCodeChange();
    ApplyFrameCodeChange();
    ApplyBeatCodeChange();
}

// ── Render ────────────────────────────────────────────────────────────────────

void DynamicMovement::Render(const RenderContext& Context)
{
    const bool grid = UseGrid && bgfx::isValid(ProgramGrid);
    const bgfx::ProgramHandle program = grid ? ProgramGrid : ProgramDirect;
    if (!bgfx::isValid(program)) return;

    const float W = (float)Context.Width;
    const float H = (float)Context.Height;
    const float beat = Context.IsBeat() ? 1.0f : 0.0f;

    // Per-frame Lua blocks.
    m_lua.SetAudioData(Context.AudioData);
    m_lua.SetEnvNumber("w", (double)W);
    m_lua.SetEnvNumber("h", (double)H);
    m_lua.SetEnvNumber("b", Context.IsBeat() ? 1.0 : 0.0);

    if (!m_inited) { m_lua.RunBlock(m_initRef, "initCode"); m_inited = true; }
    m_lua.RunBlock(m_frameRef, "frameCode");
    if (Context.IsBeat()) m_lua.RunBlock(m_beatRef, "beatCode");

    // Bridged variable values → uniform array.
    if (!m_bridged.empty())
    {
        float dyn[kMaxDyn] = { 0.0f };
        for (size_t i = 0; i < m_bridged.size(); ++i)
            dyn[i] = (float)m_lua.GetEnvNumber(m_bridged[i]);
        bgfx::setUniform(DynVarsUnif, dyn, kDynVec4);
    }

    // Source buffer: current input (0) or scratch buffer (1..8).
    const bool sameBuffer = (BufferN == 0);
    bgfx::TextureHandle srcTex = Context.InputTexture;
    if (!sameBuffer)
    {
        const int slot = std::clamp(BufferN - 1, 0, SCRATCH_BUFFER_COUNT - 1);
        srcTex = Context.FboManager->GetScratch(slot).Texture;
    }

    const float rect = RectCoords ? 1.0f : 0.0f;
    const float wrap = Wrap ? 1.0f : 0.0f;
    const float p_blendNoMoveShow[4] = { wrap, Blend ? 1.0f : 0.0f,
                                         NoMove ? 1.0f : 0.0f, ShowUV ? 1.0f : 0.0f };
    const float p_sameBi[4] = { sameBuffer ? 1.0f : 0.0f, BilinearCompat ? 1.0f : 0.0f, 0.0f, 0.0f };

    if (grid)
    {
        const float v0[4] = { W, H, beat, rect };
        const float v1[4] = { wrap, (float)GridW, (float)GridH, 0.0f };
        bgfx::setUniform(VParams0Unif, v0);
        bgfx::setUniform(VParams1Unif, v1);
        bgfx::setUniform(FParams0Unif, p_blendNoMoveShow);
        bgfx::setUniform(FParams1Unif, p_sameBi);
    }
    else
    {
        const float p0[4] = { W, H, beat, rect };
        bgfx::setUniform(Params0Unif, p0);
        bgfx::setUniform(Params1Unif, p_blendNoMoveShow);
        bgfx::setUniform(Params2Unif, p_sameBi);
    }

    // bilinearCompat samples integer texels manually → always NEAREST there.
    uint32_t srcFlags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    if (BilinearCompat || !Bilinear)
        srcFlags |= BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT;

    bgfx::setTexture(0, SourceUnif, srcTex, srcFlags);
    bgfx::setTexture(1, InputUnif,  Context.InputTexture, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    if (m_usesAudio && bgfx::isValid(Context.AudioTex))
        bgfx::setTexture(2, AudioUnif, Context.AudioTex);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    if (grid)
        bgfx::setVertexCount(GridW * GridH * 6);
    else
        bgfx::setVertexCount(3);
    bgfx::submit(Context.ViewId, program);

    Context.FboManager->Swap();
}

// ── Errors ────────────────────────────────────────────────────────────────────

std::string DynamicMovement::GetScriptError(const std::string& paramName) const
{
    if (paramName == "pixelCode") return CompileError;
    return m_lua.GetError(paramName);
}
