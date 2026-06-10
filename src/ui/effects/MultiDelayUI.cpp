#include "ui/ConfigUiRegistry.h"

#include "engine/Effect.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <string>

// Multi Delay: mode + active buffer are per-instance; the 6 buffers' delay/unit
// settings are global (shared across instances). All values round-trip through the
// effect's JSON config, so the UI stays decoupled from the shared singleton.
static void DrawMultiDelayUI(Effect* effect)
{
    nlohmann::json cfg = effect->GetConfig();

    // ── Mode ──
    {
        static const char* kModes[] = { "Disabled", "Write to buffer", "Read from buffer" };
        int mode = cfg.value("mode", 0);
        ImGui::TextUnformatted("Mode");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##mode", &mode, kModes, IM_ARRAYSIZE(kModes)))
            effect->SetConfig({ { "mode", mode } });
    }

    // ── Active buffer ──
    {
        static const char* kBufs[] = { "Buffer 1", "Buffer 2", "Buffer 3",
                                       "Buffer 4", "Buffer 5", "Buffer 6" };
        int ab = cfg.value("activebuffer", 0);
        ImGui::TextUnformatted("Active Buffer");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##activebuffer", &ab, kBufs, IM_ARRAYSIZE(kBufs)))
            effect->SetConfig({ { "activebuffer", ab } });
    }

    ImGui::SeparatorText("Buffers (shared by all Multi Delay instances)");

    if (ImGui::BeginTable("multidelay_buffers", 3,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH))
    {
        ImGui::TableSetupColumn("Buffer", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Unit",   ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Delay");
        ImGui::TableHeadersRow();

        for (int i = 0; i < 6; i++)
        {
            ImGui::PushID(i);
            const std::string ubKey = "usebeats" + std::to_string(i);
            const std::string dKey  = "delay"    + std::to_string(i);

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%d", i + 1);

            ImGui::TableNextColumn();
            int useBeats = cfg.value(ubKey, 0);
            const char* kUnits[] = { "Frames", "Beats" };
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::Combo("##unit", &useBeats, kUnits, IM_ARRAYSIZE(kUnits)))
                effect->SetConfig({ { ubKey, useBeats } });

            ImGui::TableNextColumn();
            int delay = cfg.value(dKey, 0);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##delay", &delay, 0, 200))
                effect->SetConfig({ { dKey, delay } });

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void RegisterMultiDelayUI(ConfigUiRegistry& reg)
{
    reg.Register("Multi Delay", DrawMultiDelayUI);
}
