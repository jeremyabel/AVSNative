#include "BufferSave.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"
#include "generated/spirv/fs_buffersave_blend.sc.bin.h"

void BufferSave::Init()
{
    const bgfx::ShaderHandle VertShaderFullscreen = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShaderBlit = bgfx::createShader(bgfx::copy(fs_blit_spv, sizeof(fs_blit_spv)));
    const bgfx::ShaderHandle FragShaderBlend = bgfx::createShader(bgfx::copy(fs_buffersave_blend_spv, sizeof(fs_buffersave_blend_spv)));

    BlitProgram = bgfx::createProgram(VertShaderFullscreen, FragShaderBlit, true);
    BlendProgram = bgfx::createProgram(VertShaderFullscreen, FragShaderBlend, true);

    BlitTexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    BaseTexUniform = bgfx::createUniform("s_base", bgfx::UniformType::Sampler);
    SrcTexUniform = bgfx::createUniform("s_src", bgfx::UniformType::Sampler);
    BlendParamsUniform = bgfx::createUniform("u_blendParams", bgfx::UniformType::Vec4);
}

void BufferSave::SubmitBlit(uint8_t ViewId, bgfx::TextureHandle Tex, bgfx::VertexBufferHandle QuadVB)
{
    bgfx::setTexture(0, BlitTexUniform, Tex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, QuadVB);
    bgfx::submit(ViewId, BlitProgram);
}

void BufferSave::SubmitBlend(uint8_t ViewId, bgfx::TextureHandle Base, bgfx::TextureHandle Src, bgfx::VertexBufferHandle QuadVB)
{
    const float Params[4] = { (float)Cfg.BlendMode, Cfg.BlendAmt, 0.f, 0.f };
    bgfx::setUniform(BlendParamsUniform, Params);
    bgfx::setTexture(0, BaseTexUniform, Base);
    bgfx::setTexture(1, SrcTexUniform, Src);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, QuadVB);
    bgfx::submit(ViewId, BlendProgram);
}

void BufferSave::DoSave(const RenderContext& Context)
{
    const FBOSlot& Scratch = Context.FboManager->GetScratch(Cfg.Slot);
    const uint16_t W = Context.FboManager->GetWidth();
    const uint16_t H = Context.FboManager->GetHeight();

    if (Cfg.BlendMode == 0)
    {
        // Replace: blit input → scratch (aux view executes after main, but both read from unmodified InputTexture so order doesn't matter)
        const uint8_t AuxView = Context.ViewId + 1;
        bgfx::setViewFrameBuffer(AuxView, Scratch.Fbo);
        bgfx::setViewClear(AuxView, BGFX_CLEAR_NONE, 0);
        bgfx::setViewRect(AuxView, 0, 0, W, H);
        bgfx::touch(AuxView);

        // ViewId → OutputFBO (pass-through); AuxView → Scratch.Fbo
        SubmitBlit(Context.ViewId, Context.InputTexture, Context.QuadVB);
        SubmitBlit(AuxView, Context.InputTexture, Context.QuadVB);
    }
    else
    {
        // Non-replace: blend(scratch, input) → output as intermediate,
        //              copy intermediate → scratch,
        //              overwrite output with input (pass-through).
        // Views execute in ascending ID order, so:
        //   ViewId+0 → OutputFBO : blend (intermediate)
        //   ViewId+1 → Scratch   : blit GetNext().Texture (= intermediate)
        //   ViewId+2 → OutputFBO : blit InputTexture (pass-through, overwrites blend)
        const uint8_t ScratchView = Context.ViewId + 1;
        const uint8_t PassthroughView = Context.ViewId + 2;

        bgfx::setViewFrameBuffer(ScratchView, Scratch.Fbo);
        bgfx::setViewClear(ScratchView, BGFX_CLEAR_NONE, 0);
        bgfx::setViewRect(ScratchView, 0, 0, W, H);
        bgfx::touch(ScratchView);

        bgfx::setViewFrameBuffer(PassthroughView, Context.OutputFBO);
        bgfx::setViewClear(PassthroughView, BGFX_CLEAR_NONE, 0);
        bgfx::setViewRect(PassthroughView, 0, 0, W, H);
        bgfx::touch(PassthroughView);

        SubmitBlend(Context.ViewId, Scratch.Texture, Context.InputTexture, Context.QuadVB);
        SubmitBlit(ScratchView, Context.FboManager->GetNext().Texture, Context.QuadVB);
        SubmitBlit(PassthroughView, Context.InputTexture, Context.QuadVB);
    }

    Context.FboManager->Swap();
}

void BufferSave::DoRestore(const RenderContext& Context)
{
    const FBOSlot& Scratch = Context.FboManager->GetScratch(Cfg.Slot);
    
    // blend(base=current frame, src=saved scratch) → output
    SubmitBlend(Context.ViewId, Context.InputTexture, Scratch.Texture, Context.QuadVB);
    Context.FboManager->Swap();
}

void BufferSave::Render(const RenderContext& Context)
{
    bool bDoSave = false;
    if (Cfg.Mode <= 1)
    {
        bDoSave = (Cfg.Mode == 0);
    }
    else
    {
        // Mode 2: Alternate Save/Restore — starts with Save (altPhase=false → doSave=true)
        // Mode 3: Alternate Restore/Save — starts with Restore (altPhase=false → doSave=false)
        bool saveFirst = (Cfg.Mode == 2);
        bDoSave = saveFirst ? !AltPhase : AltPhase;
        AltPhase = !AltPhase;
    }

    if (bDoSave)
    {
        DoSave(Context);
    }
    else
    {
        DoRestore(Context);
    }
}

void BufferSave::Destroy()
{
    if (bgfx::isValid(BlendParamsUniform))
        bgfx::destroy(BlendParamsUniform);
    
    if (bgfx::isValid(SrcTexUniform))
        bgfx::destroy(SrcTexUniform);
    
    if (bgfx::isValid(BaseTexUniform))
        bgfx::destroy(BaseTexUniform);
    
    if (bgfx::isValid(BlitTexUniform))
        bgfx::destroy(BlitTexUniform);
    
    if (bgfx::isValid(BlendProgram))
        bgfx::destroy(BlendProgram);
    
    if (bgfx::isValid(BlitProgram))
        bgfx::destroy(BlitProgram);

    BlendParamsUniform = BGFX_INVALID_HANDLE;
    SrcTexUniform = BGFX_INVALID_HANDLE;
    BaseTexUniform = BGFX_INVALID_HANDLE;
    BlitTexUniform = BGFX_INVALID_HANDLE;
    BlendProgram = BGFX_INVALID_HANDLE;
    BlitProgram = BGFX_INVALID_HANDLE;
}
