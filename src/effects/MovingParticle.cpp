#include "MovingParticle.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_movingparticle.sc.bin.h"

#include <algorithm>
#include <cstdlib>

void MovingParticle::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    const bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_movingparticle_spv, sizeof(fs_movingparticle_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform        = bgfx::createUniform("s_texInput",   bgfx::UniformType::Sampler);
    ParticleUniform   = bgfx::createUniform("u_particle",   bgfx::UniformType::Vec4);
    ColorUniform      = bgfx::createUniform("u_color",      bgfx::UniformType::Vec4);
    ResolutionUniform = bgfx::createUniform("u_resolution", bgfx::UniformType::Vec4);
}

void MovingParticle::Render(const RenderContext& Context)
{
    if (!bgfx::isValid(Program))
        return;

    const int Width  = Context.Width;
    const int Height = Context.Height;

    // On beat: jump attractor to random position in [-16/48, 16/48]
    if (Context.IsBeat())
    {
        AttractorX = (float)(rand() % 33 - 16) / 48.0f;
        AttractorY = (float)(rand() % 33 - 16) / 48.0f;
    }

    // Spring-damper physics (exact constants from original)
    VelX -= 0.004f * (PosX - AttractorX);
    VelY -= 0.004f * (PosY - AttractorY);
    PosX += VelX;
    PosY += VelY;
    VelX *= 0.991f;
    VelY *= 0.991f;

    // Pixel-space position
    float Ss = (float)std::min(Height / 2, (Width * 3) / 8);
    float Xp = PosX * Ss * ((float)Cfg.Distance / 32.0f) + (float)Width  * 0.5f;
    float Yp = PosY * Ss * ((float)Cfg.Distance / 32.0f) + (float)Height * 0.5f;

    // On-beat size snap, then smooth toward target
    if (Context.IsBeat() && Cfg.OnBeatSizeChange)
        CurSize = (float)Cfg.OnBeatSize;
    float DrawSize = (float)(int32_t)CurSize;
    CurSize = (float)((int32_t)((CurSize + (float)Cfg.Size) * 0.5f));

    // Center in UV space, radius in pixels
    float CenterU  = (Xp + 0.5f) / (float)Width;
    float CenterV  = (Yp + 0.5f) / (float)Height;
    float RadiusPx = DrawSize * 0.5f;

    float ParticleData[4]   = { CenterU, CenterV, RadiusPx, (float)Cfg.BlendMode };
    float ColorData[4]      = { Cfg.Color[0] / 255.0f, Cfg.Color[1] / 255.0f, Cfg.Color[2] / 255.0f, 1.0f };
    float ResolutionData[4] = { (float)Width, (float)Height, 0.0f, 0.0f };

    bgfx::setUniform(ParticleUniform,   ParticleData);
    bgfx::setUniform(ColorUniform,      ColorData);
    bgfx::setUniform(ResolutionUniform, ResolutionData);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void MovingParticle::Destroy()
{
    if (bgfx::isValid(Program))           bgfx::destroy(Program);
    if (bgfx::isValid(TexUniform))        bgfx::destroy(TexUniform);
    if (bgfx::isValid(ParticleUniform))   bgfx::destroy(ParticleUniform);
    if (bgfx::isValid(ColorUniform))      bgfx::destroy(ColorUniform);
    if (bgfx::isValid(ResolutionUniform)) bgfx::destroy(ResolutionUniform);

    Program           = BGFX_INVALID_HANDLE;
    TexUniform        = BGFX_INVALID_HANDLE;
    ParticleUniform   = BGFX_INVALID_HANDLE;
    ColorUniform      = BGFX_INVALID_HANDLE;
    ResolutionUniform = BGFX_INVALID_HANDLE;
}
