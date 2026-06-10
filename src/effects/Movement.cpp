#include "Movement.h"

#include "engine/FBOManager.h"
#include "engine/ShaderCompiler.h"

#include <bgfx/bgfx.h>

static const char* k_polarDefault =
    "r = r + (0.1 - 0.2 * d);\n"
    "d = d * 0.96;";

static const char* k_cartesianDefault =
    "x = x * 0.98;\n"
    "y = y * 0.98;";

// ---------------------------------------------------------------------------
// Pull mode (fullscreen quad) uniform descriptors
//
// Fragment UBO (binding = 1 = kSpirvFragmentBinding):
//   u_params at byte offset 0 → regIndex=0, regCount=1
//
// Sampler s_texColor at stage 0 → Vulkan binding = 0 + kSpirvBindShift = 2
//
// Type encoding:
//   Vec4 fragment    = kBgfxVec4Type(2)   | kBgfxFragmentBit(0x10)               = 0x12
//   Sampler fragment = kBgfxSamplerType(0) | kBgfxSamplerBit(0x20) | 0x10        = 0x30
// ---------------------------------------------------------------------------
static const BgfxUniformDesc k_pullUniforms[] = {
    // Name          Type   Num  RegIdx  RegCnt  TexComp TexDim TexFmt
    { "u_params",    0x12,   1,    0,      1,      0,      0,     0  },
    { "s_texColor",  0x30,   0,    2,      0,      0,      0,     2  },
};
static constexpr uint16_t k_pullUBOSize = 16; // 1 vec4 = 16 bytes

// ---------------------------------------------------------------------------
// Scatter (source map) mode uniform descriptors
//
// Vertex UBO (binding = 0 = kSpirvVertexBinding):
//   u_scatter_params at byte offset 0 → regIndex=0, regCount=1
//
// Vec4 vertex = kBgfxVec4Type(2) (no fragmentBit) = 0x02
//
// Scatter FS: s_texColor at stage 0 → binding 2 (same as pull)
// ---------------------------------------------------------------------------
static const BgfxUniformDesc k_scatterVSUniforms[] = {
    { "u_scatter_params",  0x02,   1,    0,      1,      0,      0,     0  },
};
static constexpr uint16_t k_scatterVSUBOSize = 16;

static const BgfxUniformDesc k_scatterFSUniforms[] = {
    { "s_texColor",  0x30,   0,    2,      0,      0,      0,     2  },
};
static constexpr uint16_t k_scatterFSUBOSize = 0; // no UBO in scatter FS

// Fixed scatter fragment GLSL — reads vSourceUV interpolated from the scatter VS
static const char* k_scatterFragGlsl = R"(
#version 450
layout(location = 0) in  vec2 v_sourceUV;
layout(location = 0) out vec4 bgfx_FragData0;
layout(binding = 2) uniform sampler2D s_texColor;
void main() {
    bgfx_FragData0 = texture(s_texColor, v_sourceUV);
}
)";

// Procedural fullscreen triangle VS — no vertex buffer needed.
// Generates a single triangle covering the screen via gl_VertexIndex (0,1,2).
// UV y is inverted so (0,0) = top-left, matching Vulkan texture convention.
// Compiled via glslang so it shares an interface with our GLSL fragment shaders.
static const char* k_pullVertGlsl = R"(
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

// ---------------------------------------------------------------------------

void Movement::Init()
{
    Cfg.Code = k_polarDefault;

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
    ScatterParamsUniform= bgfx::createUniform("u_scatter_params", bgfx::UniformType::Vec4);

    Compile();
}

// ---------------------------------------------------------------------------
// GLSL builders
// ---------------------------------------------------------------------------

std::string Movement::BuildPullFragGlsl() const
{
    bool IsPolar = (Cfg.Coordinates == "polar");

    const char* OutputUV = IsPolar
        ? "    vec2 NewUv = vec2(\n"
          "        0.5 + sin(r) * d * 0.70710678118,\n"
          "        0.5 + cos(r) * d * 0.70710678118);"
        : "    vec2 NewUv = vec2((x + 1.0) * 0.5, (1.0 - y) * 0.5);";

    return std::string(R"(
#version 450

layout(location = 0) in  vec4 v_texcoord0;   // bgfx varyings are always vec4
layout(location = 0) out vec4 bgfx_FragData0;

layout(std140, binding = 1) uniform _FragParams {
    vec4 u_params;  // x = time, y = wrap (bool), z = blend 50/50 (bool)
};

layout(binding = 2) uniform sampler2D s_texColor;

void main() {
    const float PI = 3.14159265358979;
    vec2 Uv = v_texcoord0.xy;

    float x = Uv.x * 2.0 - 1.0;
    float y = -(Uv.y * 2.0 - 1.0);
    float d = length(vec2(x, y)) * 0.70710678118;
    float r = atan(y, x) + PI * 0.5;
    float t = u_params.x;

    // --- user Code ---
)") + Cfg.Code + R"(
    // --- end user Code ---

)" + OutputUV + R"(

    if (u_params.y > 0.5)
        NewUv = fract(NewUv);
    else
        NewUv = clamp(NewUv, 0.0, 1.0);

    vec4 Sampled = texture(s_texColor, NewUv);

    if (u_params.z > 0.5)
        bgfx_FragData0 = mix(texture(s_texColor, Uv), Sampled, 0.5);
    else
        bgfx_FragData0 = Sampled;
}
)";
}

std::string Movement::BuildScatterVertGlsl() const
{
    bool IsPolar = (Cfg.Coordinates == "polar");

    const char* DestUV = IsPolar
        ? "    vec2 DestUV = vec2(\n"
          "        0.5 + sin(r) * d * 0.70710678118,\n"
          "        0.5 + cos(r) * d * 0.70710678118);"
        : "    vec2 DestUV = vec2((x + 1.0) * 0.5, (1.0 - y) * 0.5);";

    return std::string(R"(
#version 450

layout(location = 0) out vec2 v_sourceUV;

layout(std140, binding = 0) uniform _VertParams {
    vec4 u_scatter_params;  // x=time, y=width, z=height, w=wrap
};

void main() {
    const float PI = 3.14159265358979;
    int ix = gl_VertexIndex % int(u_scatter_params.y);
    int iy = gl_VertexIndex / int(u_scatter_params.y);
    v_sourceUV = vec2((float(ix) + 0.5) / u_scatter_params.y,
                      (float(iy) + 0.5) / u_scatter_params.z);
    float x = v_sourceUV.x * 2.0 - 1.0;
    float y = -(v_sourceUV.y * 2.0 - 1.0);
    float d = length(vec2(x, y)) * 0.70710678118;
    float r = atan(y, x) + PI * 0.5;
    float t = u_scatter_params.x;

    // --- user Code ---
)") + Cfg.Code + R"(
    // --- end user Code ---

)" + DestUV + R"(

    if (u_scatter_params.w > 0.5)
        DestUV = fract(DestUV);
    else
        DestUV = clamp(DestUV, 0.0, 1.0);

    gl_Position = vec4(DestUV.x * 2.0 - 1.0, 1.0 - DestUV.y * 2.0, 0.0, 1.0);
    gl_PointSize = 1.0;
}
)";
}

// ---------------------------------------------------------------------------
// Compile
// ---------------------------------------------------------------------------

void Movement::Compile()
{
    if (bgfx::isValid(Program))       { bgfx::destroy(Program);       Program       = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(ScatterProgram)){ bgfx::destroy(ScatterProgram); ScatterProgram= BGFX_INVALID_HANDLE; }

    // --- Pull mode ---
    {
        const std::string Glsl = BuildPullFragGlsl();
        std::vector<uint32_t> Spirv;
        if (!ShaderCompiler::GlslToSpirv(Glsl, true, Spirv, CompileError))
            return;

        const bgfx::ShaderHandle FragShader = ShaderCompiler::WrapFragmentSpirv(Spirv, k_pullUniforms, (int)std::size(k_pullUniforms), k_pullUBOSize);

        // Compile fullscreen triangle VS from GLSL so both shaders share the same
        // glslang SPIRV path — mixing precompiled HLSL→SPIRV VS with a GLSL→SPIRV FS
        // causes Vulkan pipeline creation to fail.
        std::vector<uint32_t> VertSpirv;
        std::string VertErr;
        if (!ShaderCompiler::GlslToSpirv(k_pullVertGlsl, false, VertSpirv, VertErr))
        {
            if (bgfx::isValid(FragShader)) bgfx::destroy(FragShader);
            return;
        }

        const bgfx::ShaderHandle VertShader = ShaderCompiler::WrapVertexSpirv(VertSpirv, nullptr, 0, 0);

        if (bgfx::isValid(FragShader) && bgfx::isValid(VertShader))
        {
            Program = bgfx::createProgram(VertShader, FragShader, true);
        }
        else
        {
            if (bgfx::isValid(FragShader))
                bgfx::destroy(FragShader);
            
            if (bgfx::isValid(VertShader))
                bgfx::destroy(VertShader);
        }
        CompileError.clear();
    }

    // --- Scatter mode ---
    {
        const std::string VertGlsl = BuildScatterVertGlsl();
        std::vector<uint32_t> VertSpirv;
        std::string ScatterErr;
        if (!ShaderCompiler::GlslToSpirv(VertGlsl, false, VertSpirv, ScatterErr))
            return;

        std::vector<uint32_t> FragSpirv;
        std::string FragErr;
        if (!ShaderCompiler::GlslToSpirv(k_scatterFragGlsl, true, FragSpirv, FragErr))
            return;

        const bgfx::ShaderHandle VertShader = ShaderCompiler::WrapVertexSpirv(VertSpirv, k_scatterVSUniforms, (int)std::size(k_scatterVSUniforms), k_scatterVSUBOSize);
        const bgfx::ShaderHandle FragShader = ShaderCompiler::WrapFragmentSpirv(FragSpirv, k_scatterFSUniforms, (int)std::size(k_scatterFSUniforms), k_scatterFSUBOSize);

        if (bgfx::isValid(VertShader) && bgfx::isValid(FragShader))
        {
            ScatterProgram = bgfx::createProgram(VertShader, FragShader, true);
        }
        else
        {
            if (bgfx::isValid(VertShader))
                bgfx::destroy(VertShader);
            
            if (bgfx::isValid(FragShader))
                bgfx::destroy(FragShader);
        }
    }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void Movement::Render(const RenderContext& Context)
{
    if (Cfg.OnBeatToggle && Context.IsBeat())
        Cfg.SourceMap = !Cfg.SourceMap;

    if (Cfg.SourceMap && bgfx::isValid(ScatterProgram))
        RenderScatter(Context);
    else
        RenderPull(Context);
}

void Movement::RenderPull(const RenderContext& Context)
{
    if (!bgfx::isValid(Program))
        return;

    float Params[4] = {
        (float)Context.Time,
        Cfg.Wrap  ? 1.0f : 0.0f,
        Cfg.Blend ? 1.0f : 0.0f,
        0.0f
    };
    bgfx::setUniform(ParamsUniform, Params);

    const uint32_t SamplerFlags = Cfg.Bilinear ? UINT32_MAX : (BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
    bgfx::setTexture(0, TexUniform, Context.InputTexture, SamplerFlags);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexCount(3);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Movement::RenderScatter(const RenderContext& Context)
{
    // EffectChain already cleared the output FBO to black. Scatter draws W*H points
    // with MAX (lighten) blending — overlapping points keep the brightest value,
    // unscattered pixels remain black.
    const float ScatterParams[4] = {
        (float)Context.Time,
        (float)Context.Width,
        (float)Context.Height,
        Cfg.Wrap ? 1.0f : 0.0f,
    };
    bgfx::setUniform(ScatterParamsUniform, ScatterParams);

    // Scatter always uses NEAREST — no bilinear interpolation for push mode
    bgfx::setTexture(0, TexUniform, Context.InputTexture, BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_PT_POINTS | BGFX_STATE_BLEND_LIGHTEN);
    bgfx::setVertexCount((uint32_t)(Context.Width * Context.Height));
    bgfx::submit(Context.ViewId, ScatterProgram);

    Context.FboManager->Swap();
}

// ---------------------------------------------------------------------------
// Descriptor / config
// ---------------------------------------------------------------------------

void Movement::SetCoordinateSystem(const std::string& coord)
{
    if (coord == Cfg.Coordinates)
        return;

    // Swap the built-in default code only if the user hasn't customized it.
    if (Cfg.Code == k_polarDefault || Cfg.Code == k_cartesianDefault)
        Cfg.Code = (coord == "polar") ? k_polarDefault : k_cartesianDefault;

    Cfg.Coordinates = coord;
    Compile();
}

void Movement::Destroy()
{
    if (bgfx::isValid(ScatterProgram))
        bgfx::destroy(ScatterProgram);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);
    
    if (bgfx::isValid(ScatterParamsUniform))
        bgfx::destroy(ScatterParamsUniform);
    
    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);

    ScatterProgram = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
    ScatterParamsUniform = BGFX_INVALID_HANDLE;
    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
}
