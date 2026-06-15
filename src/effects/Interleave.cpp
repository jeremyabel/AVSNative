#include "Interleave.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_interleave.sc.bin.h"

#include <algorithm>
#include <cmath>

static constexpr const char* NAME_X1 = "x";
static constexpr const char* NAME_Y1 = "y";
static constexpr const char* NAME_X2 = "x2";
static constexpr const char* NAME_Y2 = "y2";
static constexpr const char* NAME_BeatDuration  = "beatdur";
static constexpr const char* NAME_Color = "color";
static constexpr const char* NAME_EnableOnBeat = "onbeat";
static constexpr const char* NAME_OutBlend = "outBlend";

void Interleave::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_interleave_spv, sizeof(fs_interleave_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_ilColor", bgfx::UniformType::Vec4);
    GridUniform = bgfx::createUniform("u_ilGrid", bgfx::UniformType::Vec4);
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

void Interleave::Render(const RenderContext& Context)
{
    // Exponential decay toward base x/y — sc1 = (beatdur + 448) / 512
    const float sc1 = float(BeatDuration + 448) / 512.0f;
    CurX = CurX * sc1 + X * (1.0f - sc1);
    CurY = CurY * sc1 + Y * (1.0f - sc1);

    // Beat snap applied after interpolation (matches original order)
    if (EnableOnBeat && Context.IsBeat())
    {
        CurX = X2;
        CurY = Y2;
    }

    const int Tx = std::max(0, int(std::round(CurX)));
    const int Ty = std::max(0, int(std::round(CurY)));

    const float uFillColor[4] = { Color[0] / 255.0f, Color[1] / 255.0f, Color[2] / 255.0f, float(OutBlend) };
    const float uGrid[4] = { float(Tx), float(Ty), float(Context.Width), float(Context.Height) };

    bgfx::setUniform(ColorUniform, uFillColor);
    bgfx::setUniform(GridUniform, uGrid);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json Interleave::Serialize() const
{
    return 
    {
        { NAME_X1, X },
        { NAME_Y1, Y },
        { NAME_X2, X2 },
        { NAME_Y2, Y2 },
        { NAME_BeatDuration,  BeatDuration },
        { NAME_Color, JsonUtil::ColorToJson(Color) },
        { NAME_EnableOnBeat, EnableOnBeat },
        { NAME_OutBlend, OutBlend },
    };
}

void Interleave::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadFloat(j, NAME_X1, X);
    JsonUtil::ReadFloat(j, NAME_Y1, Y);
    JsonUtil::ReadFloat(j, NAME_X2, X2);
    JsonUtil::ReadFloat(j, NAME_Y2, Y2);
    JsonUtil::ReadInt(j, NAME_BeatDuration, BeatDuration);
    JsonUtil::ReadColor(j, NAME_Color, Color);
    JsonUtil::ReadBool(j, NAME_EnableOnBeat, EnableOnBeat);
    JsonUtil::ReadInt(j, NAME_OutBlend, OutBlend);

    ResetAnim();
}
