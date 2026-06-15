#include "Normalize.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_normalize_init.sc.bin.h"
#include "generated/spirv/fs_normalize_reduce.sc.bin.h"
#include "generated/spirv/fs_normalize_apply.sc.bin.h"

void Normalize::Init()
{
    m_srcSizeUnif = bgfx::createUniform("u_srcSize", bgfx::UniformType::Vec4);
    m_inputUnif = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    m_minMaxUnif = bgfx::createUniform("s_minmax", bgfx::UniformType::Sampler);

    // One shared fullscreen VS; bgfx ref-counts shaders per createProgram, so the
    // handles can be released once after all three programs are built.
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle initFS = bgfx::createShader(bgfx::copy(fs_normalize_init_spv, sizeof(fs_normalize_init_spv)));
    bgfx::ShaderHandle reduceFS = bgfx::createShader(bgfx::copy(fs_normalize_reduce_spv, sizeof(fs_normalize_reduce_spv)));
    bgfx::ShaderHandle applyFS = bgfx::createShader(bgfx::copy(fs_normalize_apply_spv, sizeof(fs_normalize_apply_spv)));

    m_initProg = bgfx::createProgram(VS, initFS, false);
    m_reduceProg = bgfx::createProgram(VS, reduceFS, false);
    m_applyProg = bgfx::createProgram(VS, applyFS, false);

    bgfx::destroy(VS);
    bgfx::destroy(initFS);
    bgfx::destroy(reduceFS);
    bgfx::destroy(applyFS);
}

void Normalize::Destroy()
{
    DestroyChain();

    if (bgfx::isValid(m_applyProg))  
        bgfx::destroy(m_applyProg);

    if (bgfx::isValid(m_reduceProg)) 
        bgfx::destroy(m_reduceProg);
    
    if (bgfx::isValid(m_initProg))   
        bgfx::destroy(m_initProg);

    if (bgfx::isValid(m_minMaxUnif))  
        bgfx::destroy(m_minMaxUnif);

    if (bgfx::isValid(m_inputUnif))   
        bgfx::destroy(m_inputUnif);

    if (bgfx::isValid(m_srcSizeUnif)) 
        bgfx::destroy(m_srcSizeUnif);

    m_applyProg = m_reduceProg = m_initProg = BGFX_INVALID_HANDLE;
    m_minMaxUnif = m_inputUnif = m_srcSizeUnif = BGFX_INVALID_HANDLE;
}

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
        bgfx::setVertexBuffer(0, Context.QuadVB);
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
        bgfx::setVertexBuffer(0, Context.QuadVB);
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
        bgfx::setVertexBuffer(0, Context.QuadVB);
        bgfx::submit(av, m_applyProg);
    }

    Context.FboManager->Swap();
}
