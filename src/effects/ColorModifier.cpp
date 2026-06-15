#include "ColorModifier.h"
#include "engine/JsonUtil.h"
#include "engine/FBOManager.h"
#include "engine/ShaderSnippets.h"

#include <algorithm>

static const std::vector<std::string> LuaBuiltIns = { "beat", "red", "green", "blue", "getspec", "getosc" };

static std::string ConcatCode(const ColorModifier& c)
{
    return c.InitCode + "\n" + c.FrameCode + "\n" + c.BeatCode;
}

std::string ColorModifier::BuildFragGlsl() const
{
    // UBO: fixed beat vec4 at offset 0, then the bridge's user-var array.
    std::string uboDecl = "layout(std140, binding = 1) uniform _FragParams {\n"
                          "    vec4 u_cmod_beat;\n"
                        + LuaBridge.EmitUboMembers()
                        + "};\n";

    // Preamble injected at the top of each channel block: unpacks beat + user vars
    // as locals so each channel evaluation starts from the same frame state.
    std::string preamble = "        float beat = u_cmod_beat.x;\n" + LuaBridge.EmitLocals();

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
layout(location = 0) in vec4 v_texcoord0;
layout(location = 0) out vec4 bgfx_FragData0;

)") + uboDecl + R"(
layout(binding = 2) uniform sampler2D s_input;

void main()
{
    vec2 uv = v_texcoord0.xy;
    vec3 c = texture(s_input, uv).rgb;
    float out_r, out_g, out_b;

)" + channelBlock("r", "red", "out_r")
   + channelBlock("g", "green", "out_g")
   + channelBlock("b", "blue", "out_b")
   + R"(
    bgfx_FragData0 = vec4(out_r, out_g, out_b, 1.0);
}
)";
}

void ColorModifier::Init()
{
    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    BeatUniform = bgfx::createUniform("u_cmod_beat", bgfx::UniformType::Vec4);
    LuaBridge.Configure("u_cmod_v");

    for (const auto& Variable : LuaBuiltIns) 
    {
        LuaContext.SeedVar(Variable);
    }

    LuaContext.CompileBlock(InitCode, NAME_InitCode, LuaRefInit);
    LuaContext.CompileBlock(FrameCode, NAME_FrameCode, LuaRefFrame);
    LuaContext.CompileBlock(BeatCode, NAME_BeatCode, LuaRefBeat);

    LuaBridge.Rescan(LuaContext, ConcatCode(*this), LuaBuiltIns);
    Recompile();

    LuaContext.SetEnvNumber("beat", 0.0);
    LuaContext.RunBlock(LuaRefInit, NAME_InitCode);
    LuaInitComplete = true;
}

void ColorModifier::Recompile()
{
    if (bgfx::isValid(Program))
    { 
        bgfx::destroy(Program); 
        Program = BGFX_INVALID_HANDLE; 
    }
    LuaBridge.DestroyUniforms();

    const std::string FragGlsl = BuildFragGlsl();
    std::vector<uint32_t> FragSpirv;
    if (!ShaderCompiler::GlslToSpirv(FragGlsl, true, FragSpirv, ShaderError))
        return;

    const uint16_t UboSize = (uint16_t)(16 + LuaBridge.UboBytes());

    std::vector<BgfxUniformDesc> Uniforms;
    Uniforms.push_back({ "u_cmod_beat", 0x12, 1, 0, 1, 0, 0, 0 });
    LuaBridge.AppendDescs(Uniforms, 16);
    Uniforms.push_back({ "s_input", 0x30, 0, 2, 0, 0, 0, 2 });

    bgfx::ShaderHandle FragShader = ShaderCompiler::WrapFragmentSpirv(FragSpirv, Uniforms.data(), (int)Uniforms.size(), UboSize);

    std::vector<uint32_t> VertSpirv;
    std::string VertErr;
    if (!ShaderCompiler::GlslToSpirv(avs::kFullscreenTriangleVertGlsl, false, VertSpirv, VertErr))
    {
        if (bgfx::isValid(FragShader)) 
            bgfx::destroy(FragShader);

        return;
    }
    bgfx::ShaderHandle VertShader = ShaderCompiler::WrapVertexSpirv(VertSpirv, nullptr, 0, 0);

    if (bgfx::isValid(FragShader) && bgfx::isValid(VertShader))
    {
        Program = bgfx::createProgram(VertShader, FragShader, true);
        ShaderError.clear();
        LuaBridge.CreateUniforms();
    }
    else
    {
        if (bgfx::isValid(FragShader)) 
            bgfx::destroy(FragShader);

        if (bgfx::isValid(VertShader)) 
            bgfx::destroy(VertShader);
    }
}

// PixelCode/InitCode change → rebuild GLSL (InitCode can add/remove user var uniforms).
void ColorModifier::RecompileMain()
{
    if (!LuaInitComplete) 
        return;

    LuaContext.CompileBlock(InitCode, NAME_InitCode, LuaRefInit);
    LuaBridge.Rescan(LuaContext, ConcatCode(*this), LuaBuiltIns);
    Recompile();
    LuaContext.SetEnvNumber("beat", 0.0);
    LuaContext.RunBlock(LuaRefInit, NAME_InitCode);
}

void ColorModifier::RecompileFrameCode()
{
    if (!LuaInitComplete) 
        return;

    LuaContext.CompileBlock(FrameCode, NAME_FrameCode, LuaRefFrame);
}

void ColorModifier::RecompileBeatCode()
{
    if (!LuaInitComplete) 
        return;

    LuaContext.CompileBlock(BeatCode, NAME_BeatCode, LuaRefBeat);
}

void ColorModifier::Render(const RenderContext& Context)
{
    if (!bgfx::isValid(Program)) 
        return;

    LuaContext.SetEnvNumber("beat", Context.IsBeat() ? 1.0 : 0.0);
    LuaContext.RunBlock(LuaRefFrame, NAME_FrameCode);

    if (Context.IsBeat())
    {
        LuaContext.RunBlock(LuaRefBeat, NAME_BeatCode);
    }

    const float BeatData[4] = { Context.IsBeat() ? 1.0f : 0.0f, 0, 0, 0 };
    bgfx::setUniform(BeatUniform, BeatData);

    LuaBridge.Upload(LuaContext);

    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexCount(3);  // fullscreen triangle; no VB — gl_VertexIndex in VS
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void ColorModifier::Destroy()
{
    if (bgfx::isValid(Program)) 
        bgfx::destroy(Program);

    if (bgfx::isValid(BeatUniform)) 
        bgfx::destroy(BeatUniform);
    
    if (bgfx::isValid(TexUniform)) 
        bgfx::destroy(TexUniform);

    LuaBridge.DestroyUniforms();

    Program = BGFX_INVALID_HANDLE;
    BeatUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    LuaInitComplete = false;
}

nlohmann::json ColorModifier::Serialize() const
{
    return 
    {
        { NAME_PixelCode, PixelCode },
        { NAME_InitCode, InitCode },
        { NAME_FrameCode, FrameCode },
        { NAME_BeatCode, BeatCode },
    };
}

void ColorModifier::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadString(j, NAME_PixelCode, PixelCode);
    JsonUtil::ReadString(j, NAME_InitCode, InitCode);
    JsonUtil::ReadString(j, NAME_FrameCode, FrameCode);
    JsonUtil::ReadString(j, NAME_BeatCode, BeatCode);

    RecompileMain();
    RecompileFrameCode();
    RecompileBeatCode();
}
