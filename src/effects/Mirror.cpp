#include "Mirror.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_mirror.sc.bin.h"

static constexpr const char* NAME_FlipX = "flipX";
static constexpr const char* NAME_FlipY = "flipY";
static constexpr const char* NAME_OnBeat = "onBeat";

void Mirror::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_mirror_spv, sizeof(fs_mirror_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);
    
    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ParamsUniform = bgfx::createUniform("u_mirrorParams", bgfx::UniformType::Vec4);
}

void Mirror::Destroy()
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

void Mirror::Render(const RenderContext& Context)
{
    if (OnBeat && Context.IsBeat())
    {
        IsBeatActive = !IsBeatActive;
    }

    bool DoFlipX = FlipX;
    bool DoFlipY = FlipY;
    if (OnBeat && IsBeatActive)
    {
        DoFlipX = !DoFlipX;
        DoFlipY = !DoFlipY;
    }

    const int Mode = (DoFlipX ? 1 : 0) | (DoFlipY ? 2 : 0);
    const float uParams[4] = { (float)Mode, 0.f, 0.f, 0.f };
    
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json Mirror::Serialize() const
{
    return 
    {
        { NAME_FlipX, FlipX },
        { NAME_FlipY, FlipY },
        { NAME_OnBeat, OnBeat },
    };
}

void Mirror::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadBool(j, NAME_FlipX, FlipX);
    JsonUtil::ReadBool(j, NAME_FlipY, FlipY);
    JsonUtil::ReadBool(j, NAME_OnBeat, OnBeat);
}
