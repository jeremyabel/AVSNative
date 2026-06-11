#include "BlitEffect.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_bliteffect.sc.bin.h"

void BlitEffect::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_bliteffect_spv, sizeof(fs_bliteffect_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);
    
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_blitParams", bgfx::UniformType::Vec4);
    FlagsUniform = bgfx::createUniform("u_blitFlags", bgfx::UniformType::Vec4);
}

void BlitEffect::Render(const RenderContext& Context)
{
    Angle += Cfg.Rotation;

    const float Params[4] = { Cfg.Zoom, Angle, Cfg.CenterX, Cfg.CenterY };

    // compat = original AVS 8-bit integer bilinear (only meaningful when Bilinear).
    const bool compat = Cfg.Bilinear && Cfg.Compat;
    const float Flags[4] = { compat ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };

    // Nearest by default (matches the win32 original); bilinear is opt-in. Using
    // bilinear in this zoom-feedback loop softens and blooms the buffer over time.
    // Compat does its own integer texelFetch blend, so it binds POINT too.
    const uint32_t PointFlags  = BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
                                 BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP;
    const uint32_t samplerFlags = (Cfg.Bilinear && !compat)
        ? (BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP)
        : PointFlags;

    bgfx::setUniform(ParamsUniform, Params);
    bgfx::setUniform(FlagsUniform, Flags);
    bgfx::setTexture(0, TexUniform, Context.InputTexture, samplerFlags);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void BlitEffect::Destroy()
{
    if (bgfx::isValid(FlagsUniform))
        bgfx::destroy(FlagsUniform);

    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);

    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    FlagsUniform = BGFX_INVALID_HANDLE;
    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}
