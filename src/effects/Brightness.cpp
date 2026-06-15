#include "Brightness.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_brightness.sc.bin.h"

static constexpr const char* NAME_Blend = "blend";
static constexpr const char* NAME_Red = "red";
static constexpr const char* NAME_Green = "green";
static constexpr const char* NAME_Blue = "blue";
static constexpr const char* NAME_Exclude = "exclude";
static constexpr const char* NAME_ExcludeColor = "excludeColor";
static constexpr const char* NAME_Distance = "distance";

// Channel → per-channel multiplier (matches smp_begin's tab_red/green/blue formula).
static float ChannelMult(int Channel)
{
    return 1.f + (Channel < 0 ? 1.f : 16.f) * ((float)Channel / 4096.f);
}

void Brightness::Init()
{
    const bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    const bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_brightness_spv, sizeof(fs_brightness_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);
    
    InputUniform = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
    MultColorUniform = bgfx::createUniform("u_mult", bgfx::UniformType::Vec4);
    ParamsUniform = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
    ExcludeColorUniform = bgfx::createUniform("u_exclude", bgfx::UniformType::Vec4);
}

void Brightness::Destroy()
{
    if (bgfx::isValid(ExcludeColorUniform))
        bgfx::destroy(ExcludeColorUniform);
    
    if (bgfx::isValid(ParamsUniform))
        bgfx::destroy(ParamsUniform);
    
    if (bgfx::isValid(MultColorUniform))
        bgfx::destroy(MultColorUniform);
    
    if (bgfx::isValid(InputUniform))
        bgfx::destroy(InputUniform);
    
    if (bgfx::isValid(Program))
        bgfx::destroy(Program);

    ExcludeColorUniform = BGFX_INVALID_HANDLE;
    ParamsUniform = BGFX_INVALID_HANDLE;
    MultColorUniform = BGFX_INVALID_HANDLE;
    InputUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

void Brightness::Render(const RenderContext& Context)
{
    const float uParams[4] = { (float)Blend, EnableExcludeColor ? 1.f : 0.f, Distance / 255.f, 0.f };
    const float uMultColor[4] = { ChannelMult(Red), ChannelMult(Green), ChannelMult(Blue), 0.f };
    const float uExcludeColor[4] = { ExcludeColor[0] / 255.f, ExcludeColor[1] / 255.f, ExcludeColor[2] / 255.f, 0.f };

    bgfx::setUniform(MultColorUniform, uMultColor);
    bgfx::setUniform(ParamsUniform, uParams);
    bgfx::setUniform(ExcludeColorUniform, uExcludeColor);
    bgfx::setTexture(0, InputUniform, Context.InputTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    Context.FboManager->Swap();
}

nlohmann::json Brightness::Serialize() const
{
    return
    {
        { NAME_Blend, Blend },
        { NAME_Red, Red },
        { NAME_Green, Green },
        { NAME_Blue, Blue },
        { NAME_Exclude, EnableExcludeColor },
        { NAME_ExcludeColor, JsonUtil::ColorToJson(ExcludeColor) },
        { NAME_Distance, Distance },
    };
}

void Brightness::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_Blend, Blend);
    JsonUtil::ReadInt(j, NAME_Red, Red);
    JsonUtil::ReadInt(j, NAME_Green, Green);
    JsonUtil::ReadInt(j, NAME_Blue, Blue);
    JsonUtil::ReadBool(j, NAME_Exclude, EnableExcludeColor);
    JsonUtil::ReadColor(j, NAME_ExcludeColor, ExcludeColor);
    JsonUtil::ReadInt(j, NAME_Distance, Distance);
}
