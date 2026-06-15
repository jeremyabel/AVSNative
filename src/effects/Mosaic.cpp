#include "Mosaic.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_mosaic.sc.bin.h"

#include <algorithm>
#include <cstdlib>

static constexpr const char* NAME_Size = "size";
static constexpr const char* NAME_EnableOnBeatSizeChange = "onBeatSizeChange";
static constexpr const char* NAME_OnBeatSize = "onBeatSize";
static constexpr const char* NAME_OnBeatDuration = "onBeatDuration";
static constexpr const char* NAME_Blend = "blend";

void Mosaic::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_mosaic_spv, sizeof(fs_mosaic_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_mosaicParams", bgfx::UniformType::Vec4);

    CurrentSize = Size;
}

void Mosaic::Destroy()
{
    if (bgfx::isValid(ParamsUniform)) 
        bgfx::destroy(ParamsUniform);

    if (bgfx::isValid(TexUniform))    
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program))       
        bgfx::destroy(Program);

    ParamsUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void Mosaic::Render(const RenderContext& Context)
{
    if (EnableOnBeatSizeChange && Context.IsBeat())
    {
        CurrentSize = OnBeatSize;
        RemainingCooldownTime = OnBeatDuration;
    }
    else if (RemainingCooldownTime == 0)
    {
        CurrentSize = Size;
    }

    if (RemainingCooldownTime > 0)
    {
        RemainingCooldownTime--;
        if (RemainingCooldownTime > 0)
        {
            const int dur = std::max(1, OnBeatDuration);
            const int a = std::abs(Size - OnBeatSize) / dur;
            CurrentSize += a * (OnBeatSize > Size ? -1 : 1);
        }
    }

    const float uParams[4] = { (float)CurrentSize, (float)Context.Width, (float)Context.Height, (float)Blend };

    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json Mosaic::Serialize() const
{
    return 
    {
        { NAME_Size, Size },
        { NAME_EnableOnBeatSizeChange, EnableOnBeatSizeChange },
        { NAME_OnBeatSize, OnBeatSize },
        { NAME_OnBeatDuration, OnBeatDuration },
        { NAME_Blend, Blend },
    };
}

void Mosaic::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Size, Size);
    JsonUtil::ReadBool(j, NAME_EnableOnBeatSizeChange, EnableOnBeatSizeChange);
    JsonUtil::ReadInt(j, NAME_OnBeatSize, OnBeatSize);
    JsonUtil::ReadInt(j, NAME_OnBeatDuration, OnBeatDuration);
    JsonUtil::ReadInt(j, NAME_Blend, Blend);
}
