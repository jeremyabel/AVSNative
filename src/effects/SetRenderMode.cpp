#include "SetRenderMode.h"

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
            ((uint32_t)(Cfg.LineWidth & 0xFF) << 16) |
            ((uint32_t)(Cfg.Alpha     & 0xFF) <<  8) |
             (uint32_t)(Cfg.BlendMode & 0xFF);
    }
}

void SetRenderMode::Destroy()
{
}
