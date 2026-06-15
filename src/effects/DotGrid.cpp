#include "DotGrid.h"

#include "engine/JsonUtil.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_dotgrid.sc.bin.h"

#include <algorithm>

static constexpr const char* NAME_Colors = "colors";
static constexpr const char* NAME_Spacing = "spacing";
static constexpr const char* NAME_SpeedX = "speedX";
static constexpr const char* NAME_SpeedY = "speedY";
static constexpr const char* NAME_BlendMode = "blendMode";

void DotGrid::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_dotgrid_spv, sizeof(fs_dotgrid_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_dgColor", bgfx::UniformType::Vec4);
    GridUniform = bgfx::createUniform("u_dgGrid", bgfx::UniformType::Vec4);
    SizeUniform = bgfx::createUniform("u_dgSize", bgfx::UniformType::Vec4);
}

void DotGrid::Destroy()
{
    if (bgfx::isValid(SizeUniform))
        bgfx::destroy(SizeUniform);
    
    if (bgfx::isValid(GridUniform))
        bgfx::destroy(GridUniform);
    
    if (bgfx::isValid(ColorUniform))
        bgfx::destroy(ColorUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    SizeUniform = BGFX_INVALID_HANDLE;
    GridUniform = BGFX_INVALID_HANDLE;
    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void DotGrid::Render(const RenderContext& Context)
{
    if (Colors.empty())
    {
        Context.FboManager->Swap();
        return;
    }

    // Advance color cycle and interpolate between adjacent entries (64 steps per pair)
    const auto [cr, cg, cb] = Colors.StepU8();

    // Compute grid pixel offsets from fixed-point scroll accumulators
    const int spacing = std::max(2, Spacing);
    const int sxRaw   = (Xp >> 8) % spacing;
    const int syRaw   = (Yp >> 8) % spacing;
    const int sx      = (sxRaw + spacing) % spacing;  // ensure non-negative
    const int sy      = (syRaw + spacing) % spacing;

    const float uColor[4] = { cr / 255.0f, cg / 255.0f, cb / 255.0f, float(BlendMode) };
    const float uGrid[4] = { float(spacing), float(sx), float(sy), 0.0f };
    const float uSize[4] = { float(Context.Width), float(Context.Height), 0.0f, 0.0f };

    bgfx::setUniform(ColorUniform, uColor);
    bgfx::setUniform(GridUniform, uGrid);
    bgfx::setUniform(SizeUniform, uSize);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);
    
    Context.FboManager->Swap();

    Xp += SpeedX;
    Yp += SpeedY;
}

nlohmann::json DotGrid::Serialize() const
{
    return
    {
        { NAME_Colors, JsonUtil::ColorsToJson(Colors) },
        { NAME_Spacing, Spacing },
        { NAME_SpeedX, SpeedX },
        { NAME_SpeedY, SpeedY },
        { NAME_BlendMode, BlendMode },
    };
}

void DotGrid::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColors(j, NAME_Colors, Colors);
    JsonUtil::ReadInt(j, NAME_Spacing, Spacing);
    JsonUtil::ReadInt(j, NAME_SpeedX, SpeedX);
    JsonUtil::ReadInt(j, NAME_SpeedY, SpeedY);
    JsonUtil::ReadInt(j, NAME_BlendMode, BlendMode);

    ResetColorCycle();
}
