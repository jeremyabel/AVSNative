#include "DynamicDistanceModifier.h"
#include "engine/JsonUtil.h"
#include "engine/FBOManager.h"
#include "engine/AudioGlsl.h"
#include "engine/ShaderSnippets.h"

#include <cmath>

static constexpr const char* NAME_Blend = "blend";
static constexpr const char* NAME_EnableBilinear = "bilinear";
static constexpr const char* NAME_BilinearCompat = "bilinearCompat";

static const std::vector<std::string> LuaBuiltIns = { "b", "getspec", "getosc", "d", "r" };

static std::string ConcatCode(const DynamicDistanceModifier& c)
{
    return c.InitCode + "\n" + c.FrameCode + "\n" + c.BeatCode;
}

// ── GLSL builder ─────────────────────────────────────────────────────────────

std::string DynamicDistanceModifier::BuildFragGlsl() const
{
    std::string uboDecl = "layout(std140, binding = 1) uniform _FragParams {\n"
                          "    vec4 u_ddm_params0;   // x=w y=h z=maxD w=beat\n"
                          "    vec4 u_ddm_params1;   // x=blend y=compat\n"
                        + LuaBridge.EmitUboMembers()
                        + "};\n";

    return std::string(R"(
#version 450
layout(location = 0) in  vec4 v_texcoord0;
layout(location = 0) out vec4 bgfx_FragData0;

)") + uboDecl + R"(
layout(binding = 2) uniform sampler2D s_input;
layout(binding = 3) uniform sampler2D s_audio;
)" + kAudioGlslFns + avs::kBilinearCompatGlsl + R"(
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

)" + LuaBridge.EmitLocals() + R"(
    // ---- pixel code ----
)" + PixelCode + R"(
    // ---- end pixel code ----

    vec2 src_uv;
    if (d_px < 0.5) {
        src_uv = vec2(0.5);
    } else {
        float scale = (d * maxD) / d_px;
        src_uv = clamp(vec2(0.5) + center * scale, 0.0, 1.0);
    }

    vec3 mapped = (u_ddm_params1.y > 0.5)
        ? BILINEAR_COMPAT(s_input, src_uv, textureSize(s_input, 0))
        : texture(s_input, src_uv).rgb;
    if (u_ddm_params1.x > 0.5) {
        vec3 orig = texture(s_input, uv).rgb;
        bgfx_FragData0 = vec4((mapped + orig) * 0.5, 1.0);
    } else {
        bgfx_FragData0 = vec4(mapped, 1.0);
    }
}
)";
}

void DynamicDistanceModifier::Init()
{
    Params1Uniform = bgfx::createUniform("u_ddm_params0", bgfx::UniformType::Vec4);
    Params2Uniform = bgfx::createUniform("u_ddm_params1", bgfx::UniformType::Vec4);
    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    AudioUniform = bgfx::createUniform("s_audio", bgfx::UniformType::Sampler);
    LuaBridge.Configure("u_ddm_v");

    for (const auto& Variable : LuaBuiltIns) 
    {
        LuaContext.SeedVar(Variable);
    }

    LuaContext.CompileBlock(InitCode, NAME_InitCode, LuaRefInit);
    LuaContext.CompileBlock(FrameCode, NAME_FrameCode, LuaRefFrame);
    LuaContext.CompileBlock(BeatCode, NAME_BeatCode, LuaRefBeat);

    LuaBridge.Rescan(LuaContext, ConcatCode(*this), LuaBuiltIns);
    Recompile();

    LuaContext.SetEnvNumber("b", 0.0);
    LuaContext.RunBlock(LuaRefInit, NAME_InitCode);
    LuaInitComplete = true;
}

void DynamicDistanceModifier::Recompile()
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

    const uint16_t UboSize = (uint16_t)(32 + LuaBridge.UboBytes());

    std::vector<BgfxUniformDesc> Uniforms;
    Uniforms.push_back({ "u_ddm_params0", 0x12, 1, 0,  1, 0, 0, 0 });
    Uniforms.push_back({ "u_ddm_params1", 0x12, 1, 16, 1, 0, 0, 0 });
    LuaBridge.AppendDescs(Uniforms, 32);
    Uniforms.push_back({ "s_input", 0x30, 0, 2, 0, 0, 0, 2 });
    Uniforms.push_back({ "s_audio", 0x30, 0, 3, 0, 0, 0, 2 });

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

void DynamicDistanceModifier::RecompileMain()
{
    if (!LuaInitComplete) return;

    LuaContext.CompileBlock(InitCode, NAME_InitCode, LuaRefInit);
    LuaBridge.Rescan(LuaContext, ConcatCode(*this), LuaBuiltIns);
    Recompile();
    LuaContext.SetEnvNumber("b", 0.0);
    LuaContext.RunBlock(LuaRefInit, NAME_InitCode);
}

void DynamicDistanceModifier::RecompileFrameCode()
{
    if (!LuaInitComplete) 
        return;

    LuaContext.CompileBlock(FrameCode, NAME_FrameCode, LuaRefFrame);
}

void DynamicDistanceModifier::RecompileBeatCode()
{
    if (!LuaInitComplete) 
        return;

    LuaContext.CompileBlock(BeatCode, NAME_BeatCode, LuaRefBeat);
}

void DynamicDistanceModifier::Render(const RenderContext& Context)
{
    LuaContext.SetAudioData(Context.AudioData);
    LuaContext.SetEnvNumber("b", Context.IsBeat() ? 1.0 : 0.0);
    LuaContext.RunBlock(LuaRefFrame, NAME_FrameCode);

    if (Context.IsBeat())
    {
        LuaContext.RunBlock(LuaRefBeat, NAME_BeatCode);
    }

    const float W = (float)Context.Width;
    const float H = (float)Context.Height;
    const float maxD = 0.5f * std::sqrt(W * W + H * H);
    const bool CompatEnabled = Bilinear && Compat;

    const float uParams1[4] = { W, H, maxD, Context.IsBeat() ? 1.0f : 0.0f };
    const float uParams2[4] = { Blend ? 1.0f : 0.0f, CompatEnabled ? 1.0f : 0.0f, 0, 0 };
    bgfx::setUniform(Params1Uniform, uParams1);
    bgfx::setUniform(Params2Uniform, uParams2);

    LuaBridge.Upload(LuaContext);

    // Compat does its own integer texelFetch blend → bind POINT. Otherwise bilinear when enabled, else nearest.
    const uint32_t inputFlags = (Bilinear && !CompatEnabled)
        ? UINT32_MAX
        : (BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
    bgfx::setTexture(0, TexUniform, Context.InputTexture, inputFlags);
    bgfx::setTexture(1, AudioUniform, Context.AudioTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexCount(3);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void DynamicDistanceModifier::Destroy()
{
    if (bgfx::isValid(Program)) 
        bgfx::destroy(Program);

    LuaBridge.DestroyUniforms();
    
    if (bgfx::isValid(AudioUniform))   
        bgfx::destroy(AudioUniform);

    if (bgfx::isValid(TexUniform))   
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Params2Uniform)) 
        bgfx::destroy(Params2Uniform);

    if (bgfx::isValid(Params1Uniform)) 
        bgfx::destroy(Params1Uniform);

    Program = BGFX_INVALID_HANDLE;
    AudioUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Params2Uniform = BGFX_INVALID_HANDLE;
    Params1Uniform = BGFX_INVALID_HANDLE;
    LuaInitComplete = false;
}

nlohmann::json DynamicDistanceModifier::Serialize() const
{
    return 
    {
        { NAME_Blend, Blend },
        { NAME_EnableBilinear, Bilinear },
        { NAME_BilinearCompat, Compat },
        { NAME_PixelCode, PixelCode },
        { NAME_InitCode, InitCode },
        { NAME_FrameCode, FrameCode },
        { NAME_BeatCode, BeatCode },
    };
}

void DynamicDistanceModifier::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadBool(j, NAME_Blend, Blend);
    JsonUtil::ReadBool(j, NAME_EnableBilinear, Bilinear);
    JsonUtil::ReadBool(j, NAME_BilinearCompat, Compat);
    JsonUtil::ReadString(j, NAME_PixelCode, PixelCode);
    JsonUtil::ReadString(j, NAME_InitCode, InitCode);
    JsonUtil::ReadString(j, NAME_FrameCode, FrameCode);
    JsonUtil::ReadString(j, NAME_BeatCode, BeatCode);

    RecompileMain();
    RecompileFrameCode();
    RecompileBeatCode();
}
