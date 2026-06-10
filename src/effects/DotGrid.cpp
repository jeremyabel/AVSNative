#include "DotGrid.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_dotgrid.sc.bin.h"

#include <algorithm>

void DotGrid::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_dotgrid_spv, sizeof(fs_dotgrid_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform   = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_dgColor",  bgfx::UniformType::Vec4);
    GridUniform  = bgfx::createUniform("u_dgGrid",   bgfx::UniformType::Vec4);
    SizeUniform  = bgfx::createUniform("u_dgSize",   bgfx::UniformType::Vec4);
}

void DotGrid::Render(const RenderContext& Context)
{
    if (Cfg.Colors.empty())
    {
        Context.FboManager->Swap();
        return;
    }

    // Advance color cycle and interpolate between adjacent entries (64 steps per pair)
    ColorPos++;
    const int cycle = (int)Cfg.Colors.size() * 64;
    if (ColorPos >= cycle) ColorPos = 0;
    const int p  = ColorPos / 64;
    const int fr = ColorPos & 63;
    const auto& c1 = Cfg.Colors[p];
    const auto& c2 = Cfg.Colors[(p + 1) % Cfg.Colors.size()];
    const int cr = (c1[0] * (63 - fr) + c2[0] * fr) / 64;
    const int cg = (c1[1] * (63 - fr) + c2[1] * fr) / 64;
    const int cb = (c1[2] * (63 - fr) + c2[2] * fr) / 64;

    // Compute grid pixel offsets from fixed-point scroll accumulators
    const int spacing = std::max(2, Cfg.Spacing);
    const int sxRaw   = (Xp >> 8) % spacing;
    const int syRaw   = (Yp >> 8) % spacing;
    const int sx      = (sxRaw + spacing) % spacing;  // ensure non-negative
    const int sy      = (syRaw + spacing) % spacing;

    const float color[4] = { cr / 255.0f, cg / 255.0f, cb / 255.0f, float(Cfg.BlendMode) };
    const float grid[4]  = { float(spacing), float(sx), float(sy), 0.0f };
    const float size[4]  = { float(Context.Width), float(Context.Height), 0.0f, 0.0f };

    bgfx::setUniform(ColorUniform, color);
    bgfx::setUniform(GridUniform,  grid);
    bgfx::setUniform(SizeUniform,  size);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    // Advance scroll after rendering (matches JS order)
    Xp += Cfg.SpeedX;
    Yp += Cfg.SpeedY;

    Context.FboManager->Swap();
}

void DotGrid::Destroy()
{
    if (bgfx::isValid(SizeUniform))  bgfx::destroy(SizeUniform);
    if (bgfx::isValid(GridUniform))  bgfx::destroy(GridUniform);
    if (bgfx::isValid(ColorUniform)) bgfx::destroy(ColorUniform);
    if (bgfx::isValid(TexUniform))   bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))      bgfx::destroy(Program);

    SizeUniform  = BGFX_INVALID_HANDLE;
    GridUniform  = BGFX_INVALID_HANDLE;
    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform   = BGFX_INVALID_HANDLE;
    Program      = BGFX_INVALID_HANDLE;
}
