#include "Blur.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blur.sc.bin.h"

static constexpr uint64_t kScratchFlags =
    BGFX_TEXTURE_RT |
    BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

void Blur::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    const bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle fs = bgfx::createShader(bgfx::copy(fs_blur_spv,       sizeof(fs_blur_spv)));
    Program       = bgfx::createProgram(vs, fs, true);
    TexUniform    = bgfx::createUniform("s_texColor",  bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_blurParams", bgfx::UniformType::Vec4);
}

void Blur::EnsureScratch(uint16_t Width, uint16_t Height)
{
    if (ScratchW == Width && ScratchH == Height)
        return;
    DestroyScratch();
    ScratchFBO = bgfx::createFrameBuffer(Width, Height, bgfx::TextureFormat::RGBA8, kScratchFlags);
    ScratchW   = Width;
    ScratchH   = Height;
}

void Blur::DestroyScratch()
{
    if (bgfx::isValid(ScratchFBO))
    {
        bgfx::destroy(ScratchFBO);
        ScratchFBO = BGFX_INVALID_HANDLE;
    }
    ScratchW = ScratchH = 0;
}

void Blur::Render(const RenderContext& Context)
{
    const uint16_t W = (uint16_t)Context.Width;
    const uint16_t H = (uint16_t)Context.Height;
    EnsureScratch(W, H);

    const int radius = (Cfg.Intensity == 3) ? 4 : (Cfg.Intensity == 2) ? 2 : 1;

    // ── View N : horizontal pass — InputTexture → ScratchFBO ─────────────────
    const uint8_t hView = Context.ViewId;
    bgfx::setViewFrameBuffer(hView, ScratchFBO);
    bgfx::setViewRect(hView, 0, 0, W, H);
    bgfx::setViewClear(hView, BGFX_CLEAR_NONE);

    float hParams[4] = { 1.0f / W, 0.0f, (float)radius, 0.0f };
    bgfx::setUniform(ParamsUniform, hParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(hView, Program);

    // ── View N+1 : vertical pass — ScratchFBO → OutputFBO ───────────────────
    const uint8_t vView = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(vView, Context.OutputFBO);
    bgfx::setViewRect(vView, 0, 0, W, H);
    bgfx::setViewClear(vView, BGFX_CLEAR_NONE);

    float vParams[4] = { 0.0f, 1.0f / H, (float)radius, 0.0f };
    bgfx::setUniform(ParamsUniform, vParams);
    bgfx::setTexture(0, TexUniform, bgfx::getTexture(ScratchFBO));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(vView, Program);

    Context.FboManager->Swap();
}

void Blur::Destroy()
{
    DestroyScratch();

    if (bgfx::isValid(ParamsUniform)) bgfx::destroy(ParamsUniform);
    if (bgfx::isValid(TexUniform))    bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))       bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform    = BGFX_INVALID_HANDLE;
    Program       = BGFX_INVALID_HANDLE;
}
