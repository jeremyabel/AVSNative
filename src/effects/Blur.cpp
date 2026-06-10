#include "Blur.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blur.sc.bin.h"

void Blur::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_blur_spv, sizeof(fs_blur_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);
    
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_blurParams", bgfx::UniformType::Vec4);
}

void Blur::EnsureScratch(uint16_t Width, uint16_t Height)
{
    if (ScratchW == Width && ScratchH == Height)
    {
        return;
    }
    
    DestroyScratch();
    
    ScratchFBO = bgfx::createFrameBuffer(Width, Height, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    ScratchW = Width;
    ScratchH = Height;
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
    const uint16_t Width = (uint16_t)Context.Width;
    const uint16_t Height = (uint16_t)Context.Height;
    EnsureScratch(Width, Height);

    const float Radius = (Cfg.Intensity == 3) ? 4.f : (Cfg.Intensity == 2) ? 2.f : 1.f;
    const float HorizParams[4] = { 1.f / (float)Width, 0.f, Radius, 0.f };
    const float VertParams[4] = { 0.f, 1.f / (float)Height, Radius, 0.f };

    // Horizontal pass: InputTexture to ScratchFBO
    const uint8_t HorizViewId = Context.ViewId;
    bgfx::setViewFrameBuffer(HorizViewId, ScratchFBO);
    bgfx::setViewRect(HorizViewId, 0, 0, Width, Height);
    bgfx::setViewClear(HorizViewId, BGFX_CLEAR_NONE);
    bgfx::setUniform(ParamsUniform, HorizParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(HorizViewId, Program);

    // Vertical pass: ScratchFBO to OutputFBO
    const uint8_t VertViewId = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(VertViewId, Context.OutputFBO);
    bgfx::setViewRect(VertViewId, 0, 0, Width, Height);
    bgfx::setViewClear(VertViewId, BGFX_CLEAR_NONE);
    bgfx::setUniform(ParamsUniform, VertParams);
    bgfx::setTexture(0, TexUniform, bgfx::getTexture(ScratchFBO));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(VertViewId, Program);

    Context.FboManager->Swap();
}

void Blur::Destroy()
{
    DestroyScratch();

    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}
