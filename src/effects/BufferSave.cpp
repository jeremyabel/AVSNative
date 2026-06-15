#include "BufferSave.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"
#include "generated/spirv/fs_buffersave_blend.sc.bin.h"

static constexpr const char* NAME_Mode = "mode";
static constexpr const char* NAME_Slot = "slot";
static constexpr const char* NAME_BlendMode = "blendMode";
static constexpr const char* NAME_BlendAmt = "blendAmt";

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

void BufferSave::SubmitBlit(uint8_t ViewId, bgfx::TextureHandle Tex, bgfx::VertexBufferHandle QuadVB)
{
    bgfx::setTexture(0, BlitTexUniform, Tex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, QuadVB);
    bgfx::submit(ViewId, BlitProgram);
}

void BufferSave::SubmitBlend(uint8_t ViewId, bgfx::TextureHandle Base, bgfx::TextureHandle Src, bgfx::VertexBufferHandle QuadVB)
{
    const float uParams[4] = { (float)BlendMode, BlendAmt, 0.f, 0.f };
    bgfx::setUniform(BlendParamsUniform, uParams);
    bgfx::setTexture(0, BaseTexUniform, Base);
    bgfx::setTexture(1, SrcTexUniform, Src);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, QuadVB);
    bgfx::submit(ViewId, BlendProgram);
}

void BufferSave::DoSave(const RenderContext& Context)
{
    const FBOSlot& Scratch = Context.FboManager->GetScratch(Slot);
    const uint16_t Width = Context.FboManager->GetWidth();
    const uint16_t Height = Context.FboManager->GetHeight();

    if (BlendMode == 0)
    {
        // Replace: blit input -> scratch (aux view executes after main, but both read from unmodified InputTexture so order doesn't matter)
        const uint8_t AuxViewId = Context.ViewId + 1;
        bgfx::setViewFrameBuffer(AuxViewId, Scratch.Fbo);
        bgfx::setViewClear(AuxViewId, BGFX_CLEAR_NONE, 0);
        bgfx::setViewRect(AuxViewId, 0, 0, Width, Height);
        bgfx::touch(AuxViewId);

        // ViewId -> OutputFBO (pass-through); AuxView → Scratch.Fbo
        SubmitBlit(Context.ViewId, Context.InputTexture, Context.QuadVB);
        SubmitBlit(AuxViewId, Context.InputTexture, Context.QuadVB);
    }
    else
    {
        // Non-replace: blend(scratch, input) → output as intermediate,
        //              copy intermediate → scratch,
        //              overwrite output with input (pass-through).
        // Views execute in ascending ID order, so:
        //   ViewId+0 -> OutputFBO : blend (intermediate)
        //   ViewId+1 -> Scratch   : blit GetNext().Texture (= intermediate)
        //   ViewId+2 -> OutputFBO : blit InputTexture (pass-through, overwrites blend)
        const uint8_t ScratchViewId = Context.ViewId + 1;
        const uint8_t PassthroughViewId = Context.ViewId + 2;

        bgfx::setViewFrameBuffer(ScratchViewId, Scratch.Fbo);
        bgfx::setViewClear(ScratchViewId, BGFX_CLEAR_NONE, 0);
        bgfx::setViewRect(ScratchViewId, 0, 0, Width, Height);
        bgfx::touch(ScratchViewId);

        bgfx::setViewFrameBuffer(PassthroughViewId, Context.OutputFBO);
        bgfx::setViewClear(PassthroughViewId, BGFX_CLEAR_NONE, 0);
        bgfx::setViewRect(PassthroughViewId, 0, 0, Width, Height);
        bgfx::touch(PassthroughViewId);

        SubmitBlend(Context.ViewId, Scratch.Texture, Context.InputTexture, Context.QuadVB);
        SubmitBlit(ScratchViewId, Context.FboManager->GetNext().Texture, Context.QuadVB);
        SubmitBlit(PassthroughViewId, Context.InputTexture, Context.QuadVB);
    }

    Context.FboManager->Swap();
}

void BufferSave::DoRestore(const RenderContext& Context)
{
    const FBOSlot& Scratch = Context.FboManager->GetScratch(Slot);
    
    // blend(base=current frame, src=saved scratch) -> output
    SubmitBlend(Context.ViewId, Context.InputTexture, Scratch.Texture, Context.QuadVB);
    Context.FboManager->Swap();
}

void BufferSave::Render(const RenderContext& Context)
{
    bool bDoSave = false;
    if (Mode <= 1)
    {
        bDoSave = (Mode == 0);
    }
    else
    {
        // Mode 2: Alternate Save/Restore — starts with Save (altPhase=false -> doSave=true)
        // Mode 3: Alternate Restore/Save — starts with Restore (altPhase=false -> doSave=false)
        bool saveFirst = (Mode == 2);
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

nlohmann::json BufferSave::Serialize() const
{
    return 
    {
        { NAME_Mode, Mode },
        { NAME_Slot, Slot },
        { NAME_BlendMode, BlendMode },
        { NAME_BlendAmt, BlendAmt },
    };
}

void BufferSave::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Mode, Mode);
    JsonUtil::ReadInt(j, NAME_Slot, Slot);
    JsonUtil::ReadInt(j, NAME_BlendMode, BlendMode);
    JsonUtil::ReadFloat(j, NAME_BlendAmt, BlendAmt);
}
