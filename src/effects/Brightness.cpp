#include "Brightness.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_brightness.sc.bin.h"

// Channel → per-channel multiplier (matches smp_begin's tab_red/green/blue formula).
static float ChannelMult(int Channel)
{
    return 1.f + (Channel < 0 ? 1.f : 16.f) * ((float)Channel / 4096.f);
}

void Brightness::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_brightness_spv, sizeof(fs_brightness_spv)));
    m_program = bgfx::createProgram(VertShader, FragShader, true);
    
    m_uInput = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    m_uMult = bgfx::createUniform("u_mult", bgfx::UniformType::Vec4);
    m_uParams = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
    m_uExclude = bgfx::createUniform("u_exclude", bgfx::UniformType::Vec4);
}

void Brightness::Render(const RenderContext& Context)
{
    const float Params[4] = { (float)Cfg.Blend, Cfg.Exclude ? 1.f : 0.f, Cfg.Distance / 255.f, 0.f };
    const float MultColor[4] = { ChannelMult(Cfg.Red), ChannelMult(Cfg.Green), ChannelMult(Cfg.Blue), 0.f };
    const float ExcludeColor[4] = { Cfg.ExcludeColor[0] / 255.f, Cfg.ExcludeColor[1] / 255.f, Cfg.ExcludeColor[2] / 255.f, 0.f };

    bgfx::setUniform(m_uMult, MultColor);
    bgfx::setUniform(m_uParams, Params);
    bgfx::setUniform(m_uExclude, ExcludeColor);
    bgfx::setTexture(0, m_uInput, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, m_program);

    Context.FboManager->Swap();
}

void Brightness::Destroy()
{
    if (bgfx::isValid(m_uExclude))
        bgfx::destroy(m_uExclude);
    
    if (bgfx::isValid(m_uParams))
        bgfx::destroy(m_uParams);
    
    if (bgfx::isValid(m_uMult))
        bgfx::destroy(m_uMult);
    
    if (bgfx::isValid(m_uInput))
        bgfx::destroy(m_uInput);
    
    if (bgfx::isValid(m_program))
        bgfx::destroy(m_program);

    m_uExclude = BGFX_INVALID_HANDLE;
    m_uParams = BGFX_INVALID_HANDLE;
    m_uMult = BGFX_INVALID_HANDLE;
    m_uInput = BGFX_INVALID_HANDLE;
    m_program = BGFX_INVALID_HANDLE;
}