#include "ui/ConfigUiRegistry.h"

#include "effects/MultiDelay.h"

#include <imgui.h>

// Multi Delay: mode + active buffer are per-instance; the 6 buffers' delay/unit
// settings are global (shared across instances), edited through MultiDelay's
// static accessors.
static void DrawMultiDelayUI(Effect* effect)
{
    auto* fx = static_cast<MultiDelay*>(effect);

    static const char* kModes[] = { "Disabled", "Write to buffer", "Read from buffer" };
    ImGui::Text("Mode");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##mode", &fx->Mode, kModes, IM_ARRAYSIZE(kModes));

    static const char* kBufs[] = { "Buffer 1", "Buffer 2", "Buffer 3", "Buffer 4", "Buffer 5", "Buffer 6" };
    ImGui::Text("Active Buffer");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##activebuffer", &fx->ActiveBuffer, kBufs, IM_ARRAYSIZE(kBufs));

    ImGui::SeparatorText("Buffers (shared by all Multi Delay instances)");

    if (ImGui::BeginTable("multidelay_buffers", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH))
    {
        ImGui::TableSetupColumn("Buffer", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Unit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Delay");
        ImGui::TableHeadersRow();

        for (int i = 0; i < 6; i++)
        {
            ImGui::PushID(i);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%d", i + 1);

            ImGui::TableNextColumn();
            int UseBeats = MultiDelay::GetBufferUseBeats(i) ? 1 : 0;
            const char* Units[] = { "Frames", "Beats" };
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::Combo("##unit", &UseBeats, Units, IM_ARRAYSIZE(Units)))
                MultiDelay::SetBufferUseBeats(i, UseBeats != 0);

            ImGui::TableNextColumn();
            int Delay = MultiDelay::GetBufferDelay(i);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##delay", &Delay, 0, 200))
                MultiDelay::SetBufferDelay(i, Delay);

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void RegisterMultiDelayUI(ConfigUiRegistry& reg)
{
    reg.Register("Multi Delay", DrawMultiDelayUI);
}
