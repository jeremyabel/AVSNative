#include "Water.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_water.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"

void Water::Init()
{
    bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle WaterFragShader = bgfx::createShader(bgfx::copy(fs_water_spv, sizeof(fs_water_spv)));
    bgfx::ShaderHandle BlitFragShader = bgfx::createShader(bgfx::copy(fs_blit_spv, sizeof(fs_blit_spv)));
    WaterProgram = bgfx::createProgram(VertShader, WaterFragShader, true);
    BlitProgram = bgfx::createProgram(VertShader, BlitFragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    PrevUniform = bgfx::createUniform("s_prevTex", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_waterParams", bgfx::UniformType::Vec4);
}

void Water::EnsurePrev(uint16_t W, uint16_t H)
{
    if (PrevW == W && PrevH == H)
        return;

    if (bgfx::isValid(PrevFBO)) bgfx::destroy(PrevFBO);
    if (bgfx::isValid(PrevTexture)) bgfx::destroy(PrevTexture);

    PrevTexture = bgfx::createTexture2D(W, H, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    PrevFBO = bgfx::createFrameBuffer(1, &PrevTexture, false);

    PrevW = W;
    PrevH = H;
}

void Water::Render(const RenderContext& Context)
{
    const uint16_t w = (uint16_t)Context.Width;
    const uint16_t h = (uint16_t)Context.Height;

    EnsurePrev(w, h);

    // Pass 1: water convolution -> output FBO (ViewId, already set up by EffectChain)
    const float params[4] = { 1.0f / float(w), 1.0f / float(h), 0.0f, 0.0f };
    bgfx::setUniform(ParamsUniform, params);
    bgfx::setTexture(0, TexUniform,  Context.InputTexture);
    bgfx::setTexture(1, PrevUniform, PrevTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, WaterProgram);

    Context.FboManager->Swap();

    // Pass 2: copy current input -> PrevFBO (ViewId+1)
    const uint8_t copyView = Context.ViewId + 1;
    bgfx::setViewFrameBuffer(copyView, PrevFBO);
    bgfx::setViewRect(copyView, 0, 0, w, h);
    bgfx::setViewClear(copyView, BGFX_CLEAR_NONE);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(copyView, BlitProgram);
}

void Water::Destroy()
{
    if (bgfx::isValid(PrevFBO))      
        bgfx::destroy(PrevFBO);

    if (bgfx::isValid(PrevTexture))  
        bgfx::destroy(PrevTexture);

    if (bgfx::isValid(ParamsUniform)) 
        bgfx::destroy(ParamsUniform);

    if (bgfx::isValid(PrevUniform))   
        bgfx::destroy(PrevUniform);

    if (bgfx::isValid(TexUniform))    
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(BlitProgram))   
        bgfx::destroy(BlitProgram);

    if (bgfx::isValid(WaterProgram))  
        bgfx::destroy(WaterProgram);

    PrevFBO = BGFX_INVALID_HANDLE;
    PrevTexture = BGFX_INVALID_HANDLE;
    ParamsUniform = BGFX_INVALID_HANDLE;
    PrevUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    BlitProgram = BGFX_INVALID_HANDLE;
    WaterProgram = BGFX_INVALID_HANDLE;

    PrevW = PrevH = 0;
}
