#include "SetRenderMode.h"

#include "engine/JsonUtil.h"

void SetRenderMode::Init()
{
}

void SetRenderMode::Render(const RenderContext& Context)
{
    // Write the packed line blend mode into the shared frame-level state.
    // Downstream effects in the same chain read Context.LineBlendMode each frame.
    // No image output and no view: EffectChain runs this in place (see ExpectedViewCount).
    if (Context.LineBlendMode)
    {
        *Context.LineBlendMode =
            ((uint32_t)(LineWidth & 0xFF) << 16) |
            ((uint32_t)(Alpha & 0xFF) <<  8) |
             (uint32_t)(BlendMode & 0xFF);
    }
}

void SetRenderMode::Destroy()
{
}

nlohmann::json SetRenderMode::Serialize() const
{
    return {
        { kBlendMode, BlendMode },
        { kLineWidth, LineWidth },
        { kAlpha,     Alpha     },
    };
}

void SetRenderMode::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, kBlendMode, BlendMode);
    JsonUtil::ReadInt(j, kLineWidth, LineWidth);
    JsonUtil::ReadInt(j, kAlpha,     Alpha);
}
