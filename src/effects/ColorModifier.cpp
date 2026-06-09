#include "ColorModifier.h"

#include "engine/FBOManager.h"

#include <algorithm>

// Built-in names that ScanVarDecls must not treat as user-declared.
static const std::vector<std::string> k_builtins = {
    "beat", "red", "green", "blue",
    "getspec", "getosc",
};

// Names for the packed user-var Vec4 uniforms (max 4 vec4s = 16 user vars).
static const char* k_varUnifNames[] = {
    "u_cmod_v0", "u_cmod_v1", "u_cmod_v2", "u_cmod_v3"
};
static_assert(std::size(k_varUnifNames) == 4, "");

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

// ── GLSL builder ─────────────────────────────────────────────────────────────

std::string ColorModifier::BuildFragGlsl() const
{
    const int numVars  = (int)m_userVars.size();
    const int numVec4s = (numVars + 3) / 4;

    // UBO declaration — only emit vec4s that hold actual user vars.
    std::string uboDecl = "layout(std140, binding = 1) uniform _FragParams {\n"
                          "    vec4 u_cmod_beat;\n";
    for (int i = 0; i < numVec4s; i++)
        uboDecl += "    vec4 u_cmod_v" + std::to_string(i) + ";\n";
    uboDecl += "};\n";

    // Preamble injected at the top of each channel block: unpacks beat + user vars
    // as locals so each channel evaluation starts from the same frame state.
    static const char* k_swizzle[] = { ".x", ".y", ".z", ".w" };
    std::string preamble = "        float beat = u_cmod_beat.x;\n";
    for (int i = 0; i < numVars; i++)
        preamble += "        float " + m_userVars[i]
                 + " = u_cmod_v" + std::to_string(i / 4)
                 + k_swizzle[i % 4] + ";\n";

    // The pixel code block is pasted three times, once per output channel.
    // Each paste gets a fresh set of locals so modifications in the red block
    // don't carry over to the green block (matching the original per-channel LUT).
    auto channelBlock = [&](const char* ch, const char* out) -> std::string {
        return std::string("    { // ") + ch + " channel\n"
             + "        float red = c." + ch + ", green = c." + ch + ", blue = c." + ch + ";\n"
             + preamble
             + "        // ---- pixel code ----\n"
             + Cfg.PixelCode + "\n"
             + "        // ---- end pixel code ----\n"
             + "        " + out + " = clamp(" + ch + ", 0.0, 1.0);\n"
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

)" + channelBlock("r", "out_r")
   + channelBlock("g", "out_g")
   + channelBlock("b", "out_b")
   + R"(
    bgfx_FragData0 = vec4(out_r, out_g, out_b, 1.0);
}
)";
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void ColorModifier::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    InputUnif = bgfx::createUniform("s_input",     bgfx::UniformType::Sampler);
    BeatUnif  = bgfx::createUniform("u_cmod_beat", bgfx::UniformType::Vec4);
    for (int i = 0; i < 4; i++)
        VarsUnif[i] = bgfx::createUniform(k_varUnifNames[i], bgfx::UniformType::Vec4);

    for (const auto& v : k_builtins) m_lua.SeedVar(v);

    m_lua.CompileBlock(Cfg.InitCode,  "initCode",  m_initRef);
    m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
    m_lua.CompileBlock(Cfg.BeatCode,  "beatCode",  m_beatRef);

    SeedUserVars();
    Recompile();

    m_lua.SetEnvNumber("beat", 0.0);
    m_lua.RunBlock(m_initRef, "initCode");
    m_inited = true;
}

void ColorModifier::Destroy()
{
    if (bgfx::isValid(Program)) bgfx::destroy(Program);
    for (int i = 3; i >= 0; i--)
        if (bgfx::isValid(VarsUnif[i])) bgfx::destroy(VarsUnif[i]);
    if (bgfx::isValid(BeatUnif))  bgfx::destroy(BeatUnif);
    if (bgfx::isValid(InputUnif)) bgfx::destroy(InputUnif);

    Program    = BGFX_INVALID_HANDLE;
    BeatUnif   = BGFX_INVALID_HANDLE;
    InputUnif  = BGFX_INVALID_HANDLE;
    for (int i = 0; i < 4; i++) VarsUnif[i] = BGFX_INVALID_HANDLE;
    m_inited   = false;
}

// ── Recompile ─────────────────────────────────────────────────────────────────

void ColorModifier::SeedUserVars()
{
    const std::string all = Cfg.InitCode + "\n" + Cfg.FrameCode + "\n" + Cfg.BeatCode;
    m_userVars = LuaRuntime::ScanVarDecls(all, k_builtins);
    for (const auto& v : m_userVars)
        m_lua.SeedVar(v);
}

void ColorModifier::Recompile()
{
    if (bgfx::isValid(Program)) { bgfx::destroy(Program); Program = BGFX_INVALID_HANDLE; }

    const std::string fragGlsl = BuildFragGlsl();
    std::vector<uint32_t> fragSpirv;
    if (!ShaderCompiler::GlslToSpirv(fragGlsl, true, fragSpirv, m_shaderError))
        return;

    const int      numVec4s = ((int)m_userVars.size() + 3) / 4;
    const uint16_t uboSize  = (uint16_t)((1 + numVec4s) * 16);

    std::vector<BgfxUniformDesc> uniforms;
    uniforms.push_back({ "u_cmod_beat", 0x12, 1, 0, 1, 0, 0, 0 });
    for (int i = 0; i < numVec4s; i++)
        uniforms.push_back({ k_varUnifNames[i], 0x12, 1, (uint16_t)(16 + i * 16), 1, 0, 0, 0 });
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
    }
    else
    {
        if (bgfx::isValid(FS)) bgfx::destroy(FS);
        if (bgfx::isValid(VS)) bgfx::destroy(VS);
    }
}

void ColorModifier::OnConfigChanged(const std::vector<std::string>& Changed)
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
        SeedUserVars();
        Recompile();
        m_lua.SetEnvNumber("beat", 0.0);
        m_lua.RunBlock(m_initRef, "initCode");
    }
    if (frameChanged) m_lua.CompileBlock(Cfg.FrameCode, "frameCode", m_frameRef);
    if (beatChanged)  m_lua.CompileBlock(Cfg.BeatCode,  "beatCode",  m_beatRef);
}

// ── Render ────────────────────────────────────────────────────────────────────

void ColorModifier::Render(const RenderContext& Context)
{
    if (!bgfx::isValid(Program)) return;

    m_lua.SetEnvNumber("beat", Context.IsBeat ? 1.0 : 0.0);
    m_lua.RunBlock(m_frameRef, "frameCode");
    if (Context.IsBeat)
        m_lua.RunBlock(m_beatRef, "beatCode");

    const float beatData[4] = { Context.IsBeat ? 1.0f : 0.0f, 0, 0, 0 };
    bgfx::setUniform(BeatUnif, beatData);

    const int numVec4s = ((int)m_userVars.size() + 3) / 4;
    for (int i = 0; i < numVec4s; i++)
    {
        float v[4] = { 0, 0, 0, 0 };
        for (int j = 0; j < 4; j++)
        {
            const int idx = i * 4 + j;
            if (idx < (int)m_userVars.size())
                v[j] = (float)m_lua.GetEnvNumber(m_userVars[idx]);
        }
        bgfx::setUniform(VarsUnif[i], v);
    }

    bgfx::setTexture(0, InputUnif, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexCount(3);  // fullscreen triangle; no VB — gl_VertexIndex in VS
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}
