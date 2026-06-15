#include "OnBeatClear.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_onbeatclear.sc.bin.h"

static constexpr const char* NAME_Color = "color";
static constexpr const char* NAME_Blend = "blend";
static constexpr const char* NAME_ClearEveryN = "nf";

void OnBeatClear::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_onbeatclear_spv, sizeof(fs_onbeatclear_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    ColorUniform = bgfx::createUniform("u_obcColor", bgfx::UniformType::Vec4);
}

void OnBeatClear::Destroy()
{
    if (bgfx::isValid(ColorUniform))
        bgfx::destroy(ColorUniform);
    
    if (bgfx::isValid(TexUniform))
        bgfx::destroy(TexUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ColorUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void OnBeatClear::Render(const RenderContext& Context)
{
    if (Context.IsBeat())
    {
        if (ClearEveryN && ++BeatsSinceLastClear >= ClearEveryN)
        {
            BeatsSinceLastClear = NonBeatFramesSinceLastClear = 0;

            const float uClearColor[4] = { Color[0] / 255.f, Color[1] / 255.f, Color[2] / 255.f, Blend ? 1.f : 0.f };

            bgfx::setUniform(ColorUniform, uClearColor);
            bgfx::setTexture(0, TexUniform, Context.InputTexture);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::setVertexBuffer(0, Context.QuadVB);
            bgfx::submit(Context.ViewId, Program);

            Context.FboManager->Swap();
        }
    }
    else
    {
        if (++NonBeatFramesSinceLastClear >= ClearEveryN)
        {
            NonBeatFramesSinceLastClear = 0;
        }
    }
}

nlohmann::json OnBeatClear::Serialize() const
{
    return 
    {
        { NAME_Color, JsonUtil::ColorToJson(Color) },
        { NAME_Blend, Blend },
        { NAME_ClearEveryN, ClearEveryN },
    };
}

void OnBeatClear::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadColor(j, NAME_Color, Color);
    JsonUtil::ReadBool(j, NAME_Blend, Blend);
    JsonUtil::ReadInt(j, NAME_ClearEveryN, ClearEveryN);
}
