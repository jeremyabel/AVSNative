#include "Timescope.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_timescope.sc.bin.h"

static constexpr const char* NAME_Channel = "channel";
static constexpr const char* NAME_Color = "color";
static constexpr const char* NAME_Blend = "blend";
static constexpr const char* NAME_Bands = "bands";

void Timescope::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_timescope_spv, sizeof(fs_timescope_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    AudioUniform = bgfx::createUniform("s_audio", bgfx::UniformType::Sampler);
    Params1Uniform = bgfx::createUniform("u_tsParams", bgfx::UniformType::Vec4);
    ColorUniform = bgfx::createUniform("u_tsColor", bgfx::UniformType::Vec4);
    Params2Uniform = bgfx::createUniform("u_tsParams2", bgfx::UniformType::Vec4);
}

void Timescope::Render(const RenderContext& Context)
{
    // Reset the scroll position when the output is resized.
    if (LastWidth != Context.Width)
    {
        LastWidth = Context.Width;
        LastPosition = 0;
    }

    // Advance the column position first
    LastPosition = (LastPosition + 1) % Context.Width;

    const float uColor[4] = { (float)Color[0], (float)Color[1], (float)Color[2], (float)Bands };
    const float uParams1[4] = { (float)LastPosition, (float)Context.Width, (float)Blend, (float)Context.Height };
    const float uParams2[4] = { (float)Channel, 0.0f, 0.0f, 0.0f };

    bgfx::setUniform(ColorUniform, uColor);
    bgfx::setUniform(Params1Uniform, uParams1);
    bgfx::setUniform(Params2Uniform, uParams2);
    bgfx::setTexture(0, TexUniform, Context.InputTexture);
    bgfx::setTexture(1, AudioUniform, Context.AudioTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

void Timescope::Destroy()
{
    if (bgfx::isValid(Params2Uniform)) 
        bgfx::destroy(Params2Uniform);

    if (bgfx::isValid(ColorUniform))   
        bgfx::destroy(ColorUniform);

    if (bgfx::isValid(Params1Uniform))  
        bgfx::destroy(Params1Uniform);

    if (bgfx::isValid(AudioUniform))   
        bgfx::destroy(AudioUniform);

    if (bgfx::isValid(TexUniform))   
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program))  
        bgfx::destroy(Program);

    Params2Uniform = BGFX_INVALID_HANDLE;
    ColorUniform = BGFX_INVALID_HANDLE;
    Params1Uniform = BGFX_INVALID_HANDLE;
    AudioUniform = BGFX_INVALID_HANDLE;
    TexUniform = BGFX_INVALID_HANDLE;
    Program  = BGFX_INVALID_HANDLE;
}

nlohmann::json Timescope::Serialize() const
{
    return 
    {
        { NAME_Channel, Channel },
        { NAME_Color, JsonUtil::ColorToJson(Color) },
        { NAME_Blend, Blend },
        { NAME_Bands, Bands },
    };
}

void Timescope::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Channel, Channel);
    JsonUtil::ReadColor(j, NAME_Color, Color);
    JsonUtil::ReadInt(j, NAME_Blend, Blend);
    JsonUtil::ReadInt(j, NAME_Bands, Bands);
}
