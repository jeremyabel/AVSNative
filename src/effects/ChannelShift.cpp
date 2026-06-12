#include "ChannelShift.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_channelshift.sc.bin.h"

#include <cstdlib>

void ChannelShift::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_channelshift_spv, sizeof(fs_channelshift_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_csParams", bgfx::UniformType::Vec4);
}

void ChannelShift::Render(const RenderContext& Context)
{
    int32_t ActiveMode = Mode;
    if (OnBeatRandom && Context.IsBeat())
    {
        ActiveMode = std::rand() % 6;
    }

    const float Params[4] = { (float)ActiveMode, 0.f, 0.f, 0.f };

    bgfx::setUniform(ParamsUniform, Params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
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

nlohmann::json ChannelShift::Serialize() const
{
    return {
        { kMode,         Mode         },
        { kOnBeatRandom, OnBeatRandom },
    };
}

void ChannelShift::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt (j, kMode,         Mode);
    JsonUtil::ReadBool(j, kOnBeatRandom, OnBeatRandom);
}
