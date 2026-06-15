#include "ChannelShift.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_channelshift.sc.bin.h"

#include <cstdlib>

static constexpr const char* NAME_Mode = "mode";
static constexpr const char* NAME_OnBeatRandom = "onBeatRandom";

void ChannelShift::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_channelshift_spv, sizeof(fs_channelshift_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_csParams", bgfx::UniformType::Vec4);
}

void ChannelShift::Destroy()
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

void ChannelShift::Render(const RenderContext& Context)
{
    int32_t ActiveMode = Mode;
    if (OnBeatRandom && Context.IsBeat())
    {
        ActiveMode = std::rand() % 6;
    }

    const float uParams[4] = { (float)ActiveMode, 0.f, 0.f, 0.f };

    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json ChannelShift::Serialize() const
{
    return
    {
        { NAME_Mode, Mode },
        { NAME_OnBeatRandom, OnBeatRandom },
    };
}

void ChannelShift::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Mode, Mode);
    JsonUtil::ReadBool(j, NAME_OnBeatRandom, OnBeatRandom);
}
