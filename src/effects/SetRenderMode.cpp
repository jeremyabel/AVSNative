#include "SetRenderMode.h"

#include "engine/JsonUtil.h"

static constexpr const char* NAME_BlendMode = "blend_mode";
static constexpr const char* NAME_LineWidth = "line_width";
static constexpr const char* NAME_Alpha = "alpha";

void SetRenderMode::Init()
{
}

void SetRenderMode::Render(const RenderContext& Context)
{
    // Write the line render mode into the shared frame-level state.
    // Downstream effects in the same chain read Context.LineMode each frame.
    // No image output and no view: EffectChain runs this in place (see ExpectedViewCount).
    if (Context.LineMode)
    {
        Context.LineMode->Width = (uint8_t)LineWidth;
        Context.LineMode->Alpha = (uint8_t)Alpha;
        Context.LineMode->Blend = (uint8_t)BlendMode;
    }
}

void SetRenderMode::Destroy()
{
}

nlohmann::json SetRenderMode::Serialize() const
{
    return 
    {
        { NAME_BlendMode, BlendMode },
        { NAME_LineWidth, LineWidth },
        { NAME_Alpha, Alpha },
    };
}

void SetRenderMode::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, NAME_BlendMode, BlendMode);
    JsonUtil::ReadInt(j, NAME_LineWidth, LineWidth);
    JsonUtil::ReadInt(j, NAME_Alpha, Alpha);
}
