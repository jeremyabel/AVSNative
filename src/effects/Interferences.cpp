#include "Interferences.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_interferences.sc.bin.h"

#include <algorithm>
#include <cmath>

static constexpr float kPi = 3.14159265358979323846f;

void Interferences::Init(bgfx::RendererType::Enum /*Renderer*/)
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_interferences_spv, sizeof(fs_interferences_spv)));
    Program = bgfx::createProgram(VS, FS, true);

    TexUniform    = bgfx::createUniform("s_texColor",   bgfx::UniformType::Sampler);
    Offsets0      = bgfx::createUniform("u_ifOffsets0", bgfx::UniformType::Vec4);
    Offsets1      = bgfx::createUniform("u_ifOffsets1", bgfx::UniformType::Vec4);
    Offsets2      = bgfx::createUniform("u_ifOffsets2", bgfx::UniformType::Vec4);
    Offsets3      = bgfx::createUniform("u_ifOffsets3", bgfx::UniformType::Vec4);
    ParamsUniform = bgfx::createUniform("u_ifParams",   bgfx::UniformType::Vec4);
}

void Interferences::Render(const RenderContext& Context)
{
    // Beat: kick the oscillator if it has completed its previous cycle
    if (Context.IsBeat() && Cfg.OnBeat && Status >= kPi)
        Status = 0.0f;

    const float s      = std::sin(Status);
    const float rotInc = Cfg.RotationInc  + (Cfg.RotationInc2  - Cfg.RotationInc)  * s;
    const float alpha  = Cfg.Alpha        + (Cfg.Alpha2         - Cfg.Alpha)         * s;
    const float dist   = Cfg.Distance     + (Cfg.Distance2      - Cfg.Distance)      * s;

    // Radially-distributed UV-space offsets
    const float a0        = (Cfg.Rotation / 255.0f) * 2.0f * kPi;
    const float angleStep = Cfg.NPoints > 0 ? (2.0f * kPi) / float(Cfg.NPoints) : 0.0f;
    const float fw        = float(Context.Width);
    const float fh        = float(Context.Height);

    float offsets[16] = {};
    for (int i = 0; i < Cfg.NPoints && i < 8; ++i)
    {
        const float a       = a0 + float(i) * angleStep;
        offsets[i * 2]     = std::cos(a) * dist / fw;
        offsets[i * 2 + 1] = std::sin(a) * dist / fh;
    }

    // Advance rotation (matches JS single-step wrap)
    Cfg.Rotation += rotInc;
    if (Cfg.Rotation >  255.0f) Cfg.Rotation -= 255.0f;
    if (Cfg.Rotation < -255.0f) Cfg.Rotation += 255.0f;

    // Advance oscillation phase
    Status += Cfg.Speed;
    if (Status > kPi)  Status = kPi;
    if (Status < -kPi) Status = kPi;

    // rgb mode is only active when nPoints is exactly 3 or 6
    const int rgbFlag = (Cfg.RGB && (Cfg.NPoints == 3 || Cfg.NPoints == 6)) ? 1 : 0;

    const float params[4] = { float(Cfg.NPoints), alpha / 255.0f, float(rgbFlag), float(Cfg.OutBlend) };

    bgfx::setUniform(Offsets0,      offsets + 0);
    bgfx::setUniform(Offsets1,      offsets + 4);
    bgfx::setUniform(Offsets2,      offsets + 8);
    bgfx::setUniform(Offsets3,      offsets + 12);
    bgfx::setUniform(ParamsUniform, params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Interferences::Destroy()
{
    if (bgfx::isValid(ParamsUniform)) bgfx::destroy(ParamsUniform);
    if (bgfx::isValid(Offsets3))      bgfx::destroy(Offsets3);
    if (bgfx::isValid(Offsets2))      bgfx::destroy(Offsets2);
    if (bgfx::isValid(Offsets1))      bgfx::destroy(Offsets1);
    if (bgfx::isValid(Offsets0))      bgfx::destroy(Offsets0);
    if (bgfx::isValid(TexUniform))    bgfx::destroy(TexUniform);
    if (bgfx::isValid(Program))       bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    Offsets3      = BGFX_INVALID_HANDLE;
    Offsets2      = BGFX_INVALID_HANDLE;
    Offsets1      = BGFX_INVALID_HANDLE;
    Offsets0      = BGFX_INVALID_HANDLE;
    TexUniform    = BGFX_INVALID_HANDLE;
    Program       = BGFX_INVALID_HANDLE;
}
