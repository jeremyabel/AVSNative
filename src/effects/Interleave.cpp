#include "Interleave.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_interleave.sc.bin.h"

#include <algorithm>
#include <cmath>

void Interleave::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_interleave_spv, sizeof(fs_interleave_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform   = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_ilColor",  bgfx::UniformType::Vec4);
    GridUniform  = bgfx::createUniform("u_ilGrid",   bgfx::UniformType::Vec4);
}

void Interleave::Render(const RenderContext& Context)
{
    // Exponential decay toward base x/y — sc1 = (beatdur + 448) / 512
    const float sc1 = float(Cfg.BeatDur + 448) / 512.0f;
    CurX = CurX * sc1 + Cfg.X * (1.0f - sc1);
    CurY = CurY * sc1 + Cfg.Y * (1.0f - sc1);

    // Beat snap applied after interpolation (matches original order)
    if (Context.IsBeat() && Cfg.OnBeat)
    {
        CurX = Cfg.X2;
        CurY = Cfg.Y2;
    }

    const int tx = std::max(0, int(std::round(CurX)));
    const int ty = std::max(0, int(std::round(CurY)));

    const float color[4] = {
        Cfg.Color[0] / 255.0f,
        Cfg.Color[1] / 255.0f,
        Cfg.Color[2] / 255.0f,
        float(Cfg.OutBlend)
    };
    const float grid[4] = {
        float(tx),
        float(ty),
        float(Context.Width),
        float(Context.Height)
    };

    bgfx::setUniform(ColorUniform, color);
    bgfx::setUniform(GridUniform,  grid);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Interleave::Destroy()
{
    if (bgfx::isValid(GridUniform))  bgfx::destroy(GridUniform);
    if (bgfx::isValid(ColorUniform)) bgfx::destroy(ColorUniform);
    if (bgfx::isValid(TexUniform))   bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))      bgfx::destroy(Program);

    GridUniform  = BGFX_INVALID_HANDLE;
    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform   = BGFX_INVALID_HANDLE;
    Program      = BGFX_INVALID_HANDLE;
}
