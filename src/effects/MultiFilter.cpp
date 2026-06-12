#include "MultiFilter.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_multifilter.sc.bin.h"

void MultiFilter::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_multifilter_spv, sizeof(fs_multifilter_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_mfParams", bgfx::UniformType::Vec4);
}

void MultiFilter::Render(const RenderContext& Context)
{
    if (ToggleOnBeat && Context.IsBeat())
    {
        ToggleState = !ToggleState;
    }

    if (!ToggleState)
    {
        return;
    }

    const float Params[4] = { float(EffectMode), 1.0f / float(Context.Width), 1.0f / float(Context.Height), 0.0f };

    bgfx::setUniform(ParamsUniform, Params);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void MultiFilter::Destroy()
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

nlohmann::json MultiFilter::Serialize() const
{
    return {
        { kEffectMode,   EffectMode   },
        { kToggleOnBeat, ToggleOnBeat },
    };
}

void MultiFilter::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt (j, kEffectMode,   EffectMode);
    JsonUtil::ReadBool(j, kToggleOnBeat, ToggleOnBeat);
}
