#include "ColorModifier.h"

#include "engine/JsonUtil.h"

#include "engine/FBOManager.h"

#include <algorithm>

// Built-in names that ScanVarDecls must not treat as user-declared.
static const std::vector<std::string> k_builtins = {
    "beat", "red", "green", "blue",
    "getspec", "getosc",
};

// Fullscreen triangle VS — same as Movement's pull VS; no vertex buffer needed.
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

static std::string ConcatCode(const ColorModifier& c)
{
    return c.InitCode + "\n" + c.FrameCode + "\n" + c.BeatCode;
}

// ── GLSL builder ─────────────────────────────────────────────────────────────

std::string ColorModifier::BuildFragGlsl() const
{
    // UBO: fixed beat vec4 at offset 0, then the bridge's user-var array.
    std::string uboDecl = "layout(std140, binding = 1) uniform _FragParams {\n"
                          "    vec4 u_cmod_beat;\n"
                        + m_bridge.EmitUboMembers()
                        + "};\n";

    // Preamble injected at the top of each channel block: unpacks beat + user vars
    // as locals so each channel evaluation starts from the same frame state.
    std::string preamble = "        float beat = u_cmod_beat.x;\n" + m_bridge.EmitLocals();

    // The pixel code block is pasted three times, once per output channel.
    // Each paste gets a fresh set of locals so modifications in the red block
    // don't carry over to the green block (matching the original per-channel LUT).
    // `ch` is the input swizzle (r/g/b); `var` is the channel variable read back as
    // the new output (red/green/blue) — matching the reference's per-channel LUT.
    auto channelBlock = [&](const char* ch, const char* var, const char* out) -> std::string {
        return std::string("    { // ") + ch + " channel\n"
             + "        float red = c." + ch + ", green = c." + ch + ", blue = c." + ch + ";\n"
             + preamble
             + "        // ---- pixel code ----\n"
             + PixelCode + "\n"
             + "        // ---- end pixel code ----\n"
             + "        " + out + " = clamp(" + var + ", 0.0, 1.0);\n"
             + "    }\n";
    };

    return std::string(R"(
#version 450
layout(location = 0) in  vec4 v_texcoord0;
layout(location = 0) out vec4 bgfx_FragData0;

)") + uboDecl + R"(
layout(binding = 2) uniform sampler2D s_input;

void main()
{
    vec2 uv = v_texcoord0.xy;
    vec3 c  = texture(s_input, uv).rgb;
    float out_r, out_g, out_b;

)" + channelBlock("r", "red",   "out_r")
   + channelBlock("g", "green", "out_g")
   + channelBlock("b", "blue",  "out_b")
   + R"(
    bgfx_FragData0 = vec4(out_r, out_g, out_b, 1.0);
}
)";
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void ColorModifier::Init()
{
    InputUnif = bgfx::createUniform("s_input",     bgfx::UniformType::Sampler);
    BeatUnif  = bgfx::createUniform("u_cmod_beat", bgfx::UniformType::Vec4);
    m_bridge.Configure("u_cmod_v");

    for (const auto& v : k_builtins) m_lua.SeedVar(v);

    m_lua.CompileBlock(InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(FrameCode, "frameCode", m_frameRef);
    m_lua.CompileBlock(BeatCode,  "beatCode",  m_beatRef);

    m_bridge.Rescan(m_lua, ConcatCode(*this), k_builtins);
    Recompile();   // builds the program and creates the bridge uniform

    m_lua.SetEnvNumber("beat", 0.0);
    m_lua.RunBlock(m_initRef, "initCode");
    m_inited = true;
}

void ColorModifier::Destroy()
{
    if (bgfx::isValid(Program)) bgfx::destroy(Program);
    m_bridge.DestroyUniforms();
    if (bgfx::isValid(BeatUnif))  bgfx::destroy(BeatUnif);
    if (bgfx::isValid(InputUnif)) bgfx::destroy(InputUnif);

    Program    = BGFX_INVALID_HANDLE;
    BeatUnif   = BGFX_INVALID_HANDLE;
    InputUnif  = BGFX_INVALID_HANDLE;
    m_inited   = false;
}

// ── Recompile ─────────────────────────────────────────────────────────────────

void ColorModifier::Recompile()
{
    if (bgfx::isValid(Program)) { bgfx::destroy(Program); Program = BGFX_INVALID_HANDLE; }
    m_bridge.DestroyUniforms();

    const std::string fragGlsl = BuildFragGlsl();
    std::vector<uint32_t> fragSpirv;
    if (!ShaderCompiler::GlslToSpirv(fragGlsl, true, fragSpirv, m_shaderError))
        return;

    const uint16_t uboSize = (uint16_t)(16 + m_bridge.UboBytes());

    std::vector<BgfxUniformDesc> uniforms;
    uniforms.push_back({ "u_cmod_beat", 0x12, 1, 0, 1, 0, 0, 0 });
    m_bridge.AppendDescs(uniforms, 16);
    uniforms.push_back({ "s_input", 0x30, 0, 2, 0, 0, 0, 2 });

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

// PixelCode/InitCode change → rebuild GLSL (InitCode can add/remove user var uniforms).
void ColorModifier::RecompileMain()
{
    if (!m_inited) return;

    m_lua.CompileBlock(InitCode, "initCode", m_initRef);
    m_bridge.Rescan(m_lua, ConcatCode(*this), k_builtins);
    Recompile();
    m_lua.SetEnvNumber("beat", 0.0);
    m_lua.RunBlock(m_initRef, "initCode");
}

void ColorModifier::RecompileFrameCode()
{
    if (!m_inited) return;
    m_lua.CompileBlock(FrameCode, "frameCode", m_frameRef);
}

void ColorModifier::RecompileBeatCode()
{
    if (!m_inited) return;
    m_lua.CompileBlock(BeatCode, "beatCode", m_beatRef);
}

nlohmann::json ColorModifier::Serialize() const
{
    return {
        { kPixelCode, PixelCode },
        { kInitCode,  InitCode  },
        { kFrameCode, FrameCode },
        { kBeatCode,  BeatCode  },
    };
}

void ColorModifier::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadString(j, kPixelCode, PixelCode);
    JsonUtil::ReadString(j, kInitCode,  InitCode);
    JsonUtil::ReadString(j, kFrameCode, FrameCode);
    JsonUtil::ReadString(j, kBeatCode,  BeatCode);

    RecompileMain();
    RecompileFrameCode();
    RecompileBeatCode();
}

// ── Render ────────────────────────────────────────────────────────────────────

void ColorModifier::Render(const RenderContext& Context)
{
    if (!bgfx::isValid(Program)) return;

    m_lua.SetEnvNumber("beat", Context.IsBeat() ? 1.0 : 0.0);
    m_lua.RunBlock(m_frameRef, "frameCode");
    if (Context.IsBeat())
        m_lua.RunBlock(m_beatRef, "beatCode");

    const float beatData[4] = { Context.IsBeat() ? 1.0f : 0.0f, 0, 0, 0 };
    bgfx::setUniform(BeatUnif, beatData);

    m_bridge.Upload(m_lua);

    bgfx::setTexture(0, InputUnif, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexCount(3);  // fullscreen triangle; no VB — gl_VertexIndex in VS
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}
