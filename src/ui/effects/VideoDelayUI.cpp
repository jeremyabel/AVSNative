#include "ui/ConfigUiRegistry.h"

#include "effects/VideoDelay.h"

#include <imgui.h>

static void DrawVideoDelayUI(Effect* base)
{
    auto* fx = static_cast<VideoDelay*>(base);

    ImGui::Checkbox("Enabled", &fx->Enabled);

    if (ImGui::Checkbox("Use Beats", &fx->UseBeats))
        fx->ApplyDelayChange();

    ImGui::TextUnformatted("Delay");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##delay", &fx->Delay, 0, fx->UseBeats ? 16 : 200))
        fx->ApplyDelayChange();
}

void RegisterVideoDelayUI(ConfigUiRegistry& reg)
{
    reg.Register("Video Delay", &DrawVideoDelayUI);
}
