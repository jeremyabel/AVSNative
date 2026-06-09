#include "DynamicDistanceModifier.h"

#include "engine/FBOManager.h"
#include "engine/AudioGlsl.h"

#include <cmath>

// Built-ins excluded from user-var scanning: `b` (beat) is supplied as a uniform,
// getspec/getosc are audio functions, and d/r are GLSL-side locals.
static const std::vector<std::string> k_builtins = {
    "b", "getspec", "getosc", "d", "r",
};

// Fullscreen triangle VS — y-flipped so v_texcoord0 (0,0) = top-left (Vulkan).
static const char* k_vertGlsl = R"(
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

static std::string ConcatCode(const DynamicDistanceModifierConfig& c)
{
    return c.InitCode + "\n" + c.FrameCode + "\n" + c.BeatCode;
}

// ── GLSL builder ─────────────────────────────────────────────────────────────

std::string DynamicDistanceModifier::BuildFragGlsl() const
{
    std::string uboDecl = "layout(std140, binding = 1) uniform _FragParams {\n"
                          "    vec4 u_ddm_params0;   // x=w y=h z=maxD w=beat\n"
                          "    vec4 u_ddm_params1;   // x=blend\n"
                        + m_bridge.EmitUboMembers()
                        + "};\n";

    return std::string(R"(
#version 450
layout(location = 0) in  vec4 v_texcoord0;
layout(location = 0) out vec4 bgfx_FragData0;

)") + uboDecl + R"(
layout(binding = 2) uniform sampler2D s_input;
layout(binding = 3) uniform sampler2D s_audio;
)" + kAudioGlslFns + R"(
void main()
{
    vec2 uv     = v_texcoord0.xy;
    vec2 center = uv - 0.5;
    float w    = u_ddm_params0.x;
    float h    = u_ddm_params0.y;
    float maxD = u_ddm_params0.z;
    float b    = u_ddm_params0.w;

    float d_px = length(vec2(center.x * w, center.y * h));
    float d = d_px / maxD;
    float r = atan(center.y, center.x);

)" + m_bridge.EmitLocals() + R"(
    // ---- pixel code ----
)" + Cfg.PixelCode + R"(
    // ---- end pixel code ----

    vec2 src_uv;
    if (d_px < 0.5) {
        src_uv = vec2(0.5);
    } else {
        float scale = (d * maxD) / d_px;
        src_uv = clamp(vec2(0.5) + center * scale, 0.0, 1.0);
    }

    vec4 mapped = texture(s_input, src_uv);
    if (u_ddm_params1.x > 0.5) {
        vec4 orig = texture(s_input, uv);
        bgfx_FragData0 = vec4((mapped.rgb + orig.rgb) * 0.5, 1.0);
    } else {
        bgfx_FragData0 = vec4(mapped.rgb, 1.0);
    }
}
)";
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void DynamicDistanceModifier::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    Params0Unif = bgfx::createUniform("u_ddm_params0", bgfx::UniformType::Vec4);
    Params1Unif = bgfx::createUniform("u_ddm_params1", bgfx::UniformType::Vec4);
    InputUnif   = bgfx::createUniform("s_input",       bgfx::UniformType::Sampler);
    AudioUnif   = bgfx::createUniform("s_audio",       bgfx::UniformType::Sampler);
    m_bridge.Configure("u_ddm_v");

    for (const auto& v : k_builtins) m_lua.SeedVar(v);

    m_lua.CompileBlock(Cfg.InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
    m_lua.CompileBlock(Cfg.BeatCode,  "beatCode",  m_beatRef);

    m_bridge.Rescan(m_lua, ConcatCode(Cfg), k_builtins);
    Recompile();

    m_lua.SetEnvNumber("b", 0.0);
    m_lua.RunBlock(m_initRef, "initCode");
    m_inited = true;
}

void DynamicDistanceModifier::Destroy()
{
    if (bgfx::isValid(Program)) bgfx::destroy(Program);
    m_bridge.DestroyUniforms();
    if (bgfx::isValid(AudioUnif))   bgfx::destroy(AudioUnif);
    if (bgfx::isValid(InputUnif))   bgfx::destroy(InputUnif);
    if (bgfx::isValid(Params1Unif)) bgfx::destroy(Params1Unif);
    if (bgfx::isValid(Params0Unif)) bgfx::destroy(Params0Unif);

    Program     = BGFX_INVALID_HANDLE;
    AudioUnif   = BGFX_INVALID_HANDLE;
    InputUnif   = BGFX_INVALID_HANDLE;
    Params1Unif = BGFX_INVALID_HANDLE;
    Params0Unif = BGFX_INVALID_HANDLE;
    m_inited    = false;
}

// ── Recompile ─────────────────────────────────────────────────────────────────

void DynamicDistanceModifier::Recompile()
{
    if (bgfx::isValid(Program)) { bgfx::destroy(Program); Program = BGFX_INVALID_HANDLE; }
    m_bridge.DestroyUniforms();

    const std::string fragGlsl = BuildFragGlsl();
    std::vector<uint32_t> fragSpirv;
    if (!ShaderCompiler::GlslToSpirv(fragGlsl, true, fragSpirv, m_shaderError))
        return;

    const uint16_t uboSize = (uint16_t)(32 + m_bridge.UboBytes());

    std::vector<BgfxUniformDesc> uniforms;
    uniforms.push_back({ "u_ddm_params0", 0x12, 1, 0,  1, 0, 0, 0 });
    uniforms.push_back({ "u_ddm_params1", 0x12, 1, 16, 1, 0, 0, 0 });
    m_bridge.AppendDescs(uniforms, 32);
    uniforms.push_back({ "s_input", 0x30, 0, 2, 0, 0, 0, 2 });
    uniforms.push_back({ "s_audio", 0x30, 0, 3, 0, 0, 0, 2 });

    bgfx::ShaderHandle FS = ShaderCompiler::WrapFragmentSpirv(
        fragSpirv, uniforms.data(), (int)uniforms.size(), uboSize);

    std::vector<uint32_t> vertSpirv;
    std::string vertErr;
    if (!ShaderCompiler::GlslToSpirv(k_vertGlsl, false, vertSpirv, vertErr))
    {
        if (bgfx::isValid(FS)) bgfx::destroy(FS);
        return;
    }
    bgfx::ShaderHandle VS = ShaderCompiler::WrapVertexSpirv(vertSpirv, nullptr, 0, 0);

    if (bgfx::isValid(FS) && bgfx::isValid(VS))
    {
        Program = bgfx::createProgram(VS, FS, true);
        m_shaderError.clear();
        m_bridge.CreateUniforms();
    }
    else
    {
        if (bgfx::isValid(FS)) bgfx::destroy(FS);
        if (bgfx::isValid(VS)) bgfx::destroy(VS);
    }
}

void DynamicDistanceModifier::OnConfigChanged(const std::vector<std::string>& Changed)
{
    if (!m_inited) return;

    bool initChanged = false, pixelChanged = false;
    bool frameChanged = false, beatChanged = false;
    for (const auto& k : Changed)
    {
        if (k == "initCode")  initChanged  = true;
        if (k == "pixelCode") pixelChanged = true;
        if (k == "frameCode") frameChanged = true;
        if (k == "beatCode")  beatChanged  = true;
    }

    // pixelCode or initCode change → rebuild GLSL (initCode can add/remove user var uniforms).
    if (pixelChanged || initChanged)
    {
        m_lua.CompileBlock(Cfg.InitCode, "initCode", m_initRef);
        m_bridge.Rescan(m_lua, ConcatCode(Cfg), k_builtins);
        Recompile();
        m_lua.SetEnvNumber("b", 0.0);
        m_lua.RunBlock(m_initRef, "initCode");
    }
    if (frameChanged) m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
    if (beatChanged)  m_lua.CompileBlock(Cfg.BeatCode,  "beatCode",  m_beatRef);
}

// ── Render ────────────────────────────────────────────────────────────────────

void DynamicDistanceModifier::Render(const RenderContext& Context)
{
    if (!bgfx::isValid(Program)) return;

    const bool isBeat = Context.IsBeat();

    m_lua.SetAudioData(Context.AudioData);
    m_lua.SetEnvNumber("b", isBeat ? 1.0 : 0.0);
    m_lua.RunBlock(m_frameRef, "frameCode");
    if (isBeat)
        m_lua.RunBlock(m_beatRef, "beatCode");

    const float w    = (float)Context.Width;
    const float h    = (float)Context.Height;
    const float maxD = 0.5f * std::sqrt(w * w + h * h);

    const float params0[4] = { w, h, maxD, isBeat ? 1.0f : 0.0f };
    const float params1[4] = { Cfg.Blend ? 1.0f : 0.0f, 0, 0, 0 };
    bgfx::setUniform(Params0Unif, params0);
    bgfx::setUniform(Params1Unif, params1);

    m_bridge.Upload(m_lua);

    const uint32_t inputFlags = Cfg.Bilinear
        ? UINT32_MAX
        : (BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
    bgfx::setTexture(0, InputUnif, Context.InputTexture, inputFlags);
    bgfx::setTexture(1, AudioUnif, Context.AudioTex);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexCount(3);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}
