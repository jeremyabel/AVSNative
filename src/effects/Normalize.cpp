#include "Normalize.h"

#include "engine/FBOManager.h"
#include "engine/ShaderCompiler.h"

// Fullscreen triangle VS — same as Movement/ColorModifier.
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

// Init pass: color input → per-pixel (min,max) across 2×2 block.
// Output R = global min of all channels in block, G = global max.
static const char* k_initFragGlsl = R"(
#version 450
layout(location = 0) in  vec4 v_texcoord0;
layout(location = 0) out vec4 bgfx_FragData0;
layout(std140, binding = 1) uniform _FragParams { vec4 u_srcSize; };
layout(binding = 2) uniform sampler2D s_input;
void main() {
    ivec2 b  = ivec2(gl_FragCoord.xy) * 2;
    ivec2 sz = ivec2(int(u_srcSize.x) - 1, int(u_srcSize.y) - 1);
    vec3 s00 = texelFetch(s_input, clamp(b,                ivec2(0), sz), 0).rgb;
    vec3 s10 = texelFetch(s_input, clamp(b + ivec2(1, 0),  ivec2(0), sz), 0).rgb;
    vec3 s01 = texelFetch(s_input, clamp(b + ivec2(0, 1),  ivec2(0), sz), 0).rgb;
    vec3 s11 = texelFetch(s_input, clamp(b + ivec2(1, 1),  ivec2(0), sz), 0).rgb;
    vec3 mn3 = min(min(s00, s10), min(s01, s11));
    vec3 mx3 = max(max(s00, s10), max(s01, s11));
    bgfx_FragData0 = vec4(min(mn3.r, min(mn3.g, mn3.b)),
                          max(mx3.r, max(mx3.g, mx3.b)),
                          0.0, 1.0);
}
)";

// Reduce pass: (min,max) texture → half-res (min,max) by combining 2×2 blocks.
static const char* k_reduceFragGlsl = R"(
#version 450
layout(location = 0) in  vec4 v_texcoord0;
layout(location = 0) out vec4 bgfx_FragData0;
layout(std140, binding = 1) uniform _FragParams { vec4 u_srcSize; };
layout(binding = 2) uniform sampler2D s_input;
void main() {
    ivec2 b  = ivec2(gl_FragCoord.xy) * 2;
    ivec2 sz = ivec2(int(u_srcSize.x) - 1, int(u_srcSize.y) - 1);
    vec4 s00 = texelFetch(s_input, clamp(b,                ivec2(0), sz), 0);
    vec4 s10 = texelFetch(s_input, clamp(b + ivec2(1, 0),  ivec2(0), sz), 0);
    vec4 s01 = texelFetch(s_input, clamp(b + ivec2(0, 1),  ivec2(0), sz), 0);
    vec4 s11 = texelFetch(s_input, clamp(b + ivec2(1, 1),  ivec2(0), sz), 0);
    bgfx_FragData0 = vec4(min(min(s00.r, s10.r), min(s01.r, s11.r)),
                          max(max(s00.g, s10.g), max(s01.g, s11.g)),
                          0.0, 1.0);
}
)";

// Apply pass: remap input using the 1×1 global (min,max) result.
// If max == min the image is flat → output black (matches original AVS behaviour).
static const char* k_applyFragGlsl = R"(
#version 450
layout(location = 0) in  vec4 v_texcoord0;
layout(location = 0) out vec4 bgfx_FragData0;
layout(binding = 2) uniform sampler2D s_input;
layout(binding = 3) uniform sampler2D s_minmax;
void main() {
    vec2 uv   = v_texcoord0.xy;
    vec4 mm   = texture(s_minmax, vec2(0.5));
    float mn  = mm.r, mx = mm.g;
    vec4  px  = texture(s_input, uv);
    float rng = mx - mn;
    if (rng > 0.0)
        px.rgb = clamp((px.rgb - mn) / rng, 0.0, 1.0);
    else
        px.rgb = vec3(0.0);
    bgfx_FragData0 = px;
}
)";

// ── Helpers ───────────────────────────────────────────────────────────────────

void Normalize::CompileProgram(const char* FragGlsl, const BgfxUniformDesc* Uniforms,
                               int UniformCount, uint16_t UboSize,
                               bgfx::ProgramHandle& Out) const
{
    std::vector<uint32_t> vs, fs;
    std::string err;

    if (!ShaderCompiler::GlslToSpirv(k_vertGlsl, false, vs, err)) return;
    if (!ShaderCompiler::GlslToSpirv(FragGlsl,   true,  fs, err)) return;

    bgfx::ShaderHandle VS = ShaderCompiler::WrapVertexSpirv(vs, nullptr, 0, 0);
    bgfx::ShaderHandle FS = ShaderCompiler::WrapFragmentSpirv(fs, Uniforms, UniformCount, UboSize);

    if (bgfx::isValid(VS) && bgfx::isValid(FS))
        Out = bgfx::createProgram(VS, FS, true);
    else
    {
        if (bgfx::isValid(VS)) bgfx::destroy(VS);
        if (bgfx::isValid(FS)) bgfx::destroy(FS);
    }
}

// ── Init / Destroy ────────────────────────────────────────────────────────────

void Normalize::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    m_srcSizeUnif = bgfx::createUniform("u_srcSize", bgfx::UniformType::Vec4);
    m_inputUnif   = bgfx::createUniform("s_input",   bgfx::UniformType::Sampler);
    m_minMaxUnif  = bgfx::createUniform("s_minmax",  bgfx::UniformType::Sampler);

    // Init + reduce: one UBO (u_srcSize) + one sampler (s_input).
    const BgfxUniformDesc initReduceUniforms[2] = {
        { "u_srcSize", kBgfxVec4Type | kBgfxFragmentBit,
          1, 0, 1, 0, 0, 0 },
        { "s_input", kBgfxSamplerType | kBgfxFragmentBit | kBgfxSamplerBit,
          0, kSpirvBindShift + 0, 0, 0, 0, kTexFmt2DSampler },
    };
    CompileProgram(k_initFragGlsl,   initReduceUniforms, 2, 16, m_initProg);
    CompileProgram(k_reduceFragGlsl, initReduceUniforms, 2, 16, m_reduceProg);

    // Apply: two samplers (s_input at slot 0, s_minmax at slot 1), no UBO.
    const BgfxUniformDesc applyUniforms[2] = {
        { "s_input",  kBgfxSamplerType | kBgfxFragmentBit | kBgfxSamplerBit,
          0, kSpirvBindShift + 0, 0, 0, 0, kTexFmt2DSampler },
        { "s_minmax", kBgfxSamplerType | kBgfxFragmentBit | kBgfxSamplerBit,
          0, kSpirvBindShift + 1, 0, 0, 0, kTexFmt2DSampler },
    };
    CompileProgram(k_applyFragGlsl, applyUniforms, 2, 0, m_applyProg);
}

void Normalize::Destroy()
{
    DestroyChain();

    if (bgfx::isValid(m_applyProg))  bgfx::destroy(m_applyProg);
    if (bgfx::isValid(m_reduceProg)) bgfx::destroy(m_reduceProg);
    if (bgfx::isValid(m_initProg))   bgfx::destroy(m_initProg);

    if (bgfx::isValid(m_minMaxUnif))  bgfx::destroy(m_minMaxUnif);
    if (bgfx::isValid(m_inputUnif))   bgfx::destroy(m_inputUnif);
    if (bgfx::isValid(m_srcSizeUnif)) bgfx::destroy(m_srcSizeUnif);

    m_applyProg = m_reduceProg = m_initProg = BGFX_INVALID_HANDLE;
    m_minMaxUnif = m_inputUnif = m_srcSizeUnif = BGFX_INVALID_HANDLE;
}

// ── Reduction chain management ────────────────────────────────────────────────

void Normalize::EnsureChain(int W, int H)
{
    if (m_chainW == W && m_chainH == H) return;
    DestroyChain();

    const uint64_t flags = BGFX_TEXTURE_RT
                         | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
                         | BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP;

    int cw = std::max(1, (W + 1) / 2);
    int ch = std::max(1, (H + 1) / 2);
    for (;;)
    {
        Level lv;
        lv.W   = cw;
        lv.H   = ch;
        lv.Tex = bgfx::createTexture2D((uint16_t)cw, (uint16_t)ch,
                                        false, 1, bgfx::TextureFormat::RGBA8, flags);
        lv.Fbo = bgfx::createFrameBuffer(1, &lv.Tex, false);
        m_chain.push_back(lv);

        if (cw == 1 && ch == 1) break;
        cw = std::max(1, (cw + 1) / 2);
        ch = std::max(1, (ch + 1) / 2);
    }

    m_chainW = W;
    m_chainH = H;
}

void Normalize::DestroyChain()
{
    for (auto& lv : m_chain)
    {
        if (bgfx::isValid(lv.Fbo)) bgfx::destroy(lv.Fbo);
        if (bgfx::isValid(lv.Tex)) bgfx::destroy(lv.Tex);
    }
    m_chain.clear();
    m_chainW = m_chainH = -1;
}

// ── Render ────────────────────────────────────────────────────────────────────

void Normalize::Render(const RenderContext& Context)
{
    if (!bgfx::isValid(m_initProg) ||
        !bgfx::isValid(m_reduceProg) ||
        !bgfx::isValid(m_applyProg)) return;

    const int w = Context.Width, h = Context.Height;
    EnsureChain(w, h);

    // View layout within the reserved block starting at Context.ViewId:
    //   ViewId + 0          : init pass → m_chain[0]
    //   ViewId + 1 .. N-1   : reduce passes → m_chain[1..N-1]
    //   ViewId + N          : apply pass → Context.OutputFBO
    const uint8_t base = Context.ViewId;
    const int     N    = (int)m_chain.size();

    // Init pass: sample color input in 2×2 blocks → chain[0] (W/2 × H/2).
    {
        const float sz[4] = { (float)w, (float)h, 0.0f, 0.0f };
        bgfx::setViewFrameBuffer(base,      m_chain[0].Fbo);
        bgfx::setViewRect      (base, 0, 0, (uint16_t)m_chain[0].W, (uint16_t)m_chain[0].H);
        bgfx::setUniform(m_srcSizeUnif, sz);
        bgfx::setTexture(0, m_inputUnif, Context.InputTexture);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::setVertexCount(3);
        bgfx::submit(base, m_initProg);
    }

    // Reduce passes: halve resolution until 1×1.
    for (int i = 1; i < N; i++)
    {
        const uint8_t rv = base + (uint8_t)i;
        const float   sz[4] = { (float)m_chain[i-1].W, (float)m_chain[i-1].H, 0.0f, 0.0f };
        bgfx::setViewFrameBuffer(rv,      m_chain[i].Fbo);
        bgfx::setViewRect      (rv, 0, 0, (uint16_t)m_chain[i].W, (uint16_t)m_chain[i].H);
        bgfx::setUniform(m_srcSizeUnif, sz);
        bgfx::setTexture(0, m_inputUnif, m_chain[i-1].Tex);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::setVertexCount(3);
        bgfx::submit(rv, m_reduceProg);
    }

    // Apply pass: remap original input using the 1×1 global min/max.
    {
        const uint8_t av = base + (uint8_t)N;
        bgfx::setViewFrameBuffer(av,      Context.OutputFBO);
        bgfx::setViewRect      (av, 0, 0, (uint16_t)w, (uint16_t)h);
        bgfx::setTexture(0, m_inputUnif,  Context.InputTexture);
        bgfx::setTexture(1, m_minMaxUnif, m_chain.back().Tex);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::setVertexCount(3);
        bgfx::submit(av, m_applyProg);
    }

    Context.FboManager->Swap();
}
