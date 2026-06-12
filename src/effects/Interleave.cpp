#include "Interleave.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_interleave.sc.bin.h"

#include <algorithm>
#include <cmath>

void Interleave::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_interleave_spv, sizeof(fs_interleave_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_ilColor", bgfx::UniformType::Vec4);
    GridUniform = bgfx::createUniform("u_ilGrid", bgfx::UniformType::Vec4);
}

void Interleave::Render(const RenderContext& Context)
{
    // Exponential decay toward base x/y — sc1 = (beatdur + 448) / 512
    const float sc1 = float(BeatDur + 448) / 512.0f;
    CurX = CurX * sc1 + X * (1.0f - sc1);
    CurY = CurY * sc1 + Y * (1.0f - sc1);

    // Beat snap applied after interpolation (matches original order)
    if (Context.IsBeat() && OnBeat)
    {
        CurX = X2;
        CurY = Y2;
    }

    const int Tx = std::max(0, int(std::round(CurX)));
    const int Ty = std::max(0, int(std::round(CurY)));

    const float FillColor[4] = { Color[0] / 255.0f, Color[1] / 255.0f, Color[2] / 255.0f, float(OutBlend) };
    const float Grid[4] = { float(Tx), float(Ty), float(Context.Width), float(Context.Height) };

    bgfx::setUniform(ColorUniform, FillColor);
    bgfx::setUniform(GridUniform, Grid);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Interleave::Destroy()
{
    if (bgfx::isValid(GridUniform))
        bgfx::destroy(GridUniform);
    
    if (bgfx::isValid(ColorUniform))
        bgfx::destroy(ColorUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    GridUniform = BGFX_INVALID_HANDLE;
    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json Interleave::Serialize() const
{
    return {
        { kX,        X        },
        { kY,        Y        },
        { kX2,       X2       },
        { kY2,       Y2       },
        { kBeatDur,  BeatDur  },
        { kColor,    JsonUtil::ColorToJson(Color) },
        { kOnBeat,   OnBeat   },
        { kOutBlend, OutBlend },
    };
}

void Interleave::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadFloat(j, kX,        X);
    JsonUtil::ReadFloat(j, kY,        Y);
    JsonUtil::ReadFloat(j, kX2,       X2);
    JsonUtil::ReadFloat(j, kY2,       Y2);
    JsonUtil::ReadInt  (j, kBeatDur,  BeatDur);
    JsonUtil::ReadColor(j, kColor,    Color);
    JsonUtil::ReadBool (j, kOnBeat,   OnBeat);
    JsonUtil::ReadInt  (j, kOutBlend, OutBlend);

    ResetAnim();
}
