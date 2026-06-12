#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Strobe.h"

#include <imgui.h>

#include <algorithm>

static void DrawStrobeUI(Effect* effect)
{
    auto* s = static_cast<Strobe*>(effect);

    // Speed: higher = faster = shorter interval. Presented inverted so dragging
    // right speeds up; the right end is kMinInterval (a strobe every other frame).
    int speed = Strobe::kMaxInterval - std::clamp(s->Interval, Strobe::kMinInterval,
                                                  Strobe::kMaxInterval);
    ImGui::TextUnformatted("Speed");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##speed", &speed, 0, Strobe::kMaxInterval - Strobe::kMinInterval,
                         "%d"))
        s->Interval = Strobe::kMaxInterval - speed;
    ImGui::SameLine();
    ImGui::TextDisabled("(every %d frames)", std::max(Strobe::kMinInterval, s->Interval));

    // Duration: frames the color stays on per strobe.
    ImGui::TextUnformatted("Duration (frames)");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputInt("##duration", &s->Duration))
        s->Duration = std::max(1, s->Duration);

    // Colors: cycled one per strobe.
    ImGui::SeparatorText("Colors");
    ConfigUi::ColorsEdit("##colors", s->Colors);

    // Trigger.
    ImGui::SeparatorText("Trigger");
    const char* modeOpts[] = { "Always On", "Keyboard (hold)" };
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##mode", modeOpts[std::clamp(s->TriggerMode, 0, 1)]))
    {
        for (int i = 0; i < 2; ++i)
            if (ImGui::Selectable(modeOpts[i], s->TriggerMode == i)) s->TriggerMode = i;
        ImGui::EndCombo();
    }

    if (s->TriggerMode == 1)
    {
        ImGui::TextUnformatted("Hold key");
        ImGui::SameLine();
        ConfigUi::KeyCaptureButton("##trigkey", s, 0, s->TriggerKey);
    }
}

void RegisterStrobeUI(ConfigUiRegistry& reg)
{
    reg.Register("Strobe", DrawStrobeUI);
}
