#include "SlidersPanel.h"

#include "engine/Engine.h"
#include "engine/GlobalSlider.h"

#include <imgui.h>

#include <cstdio>

void SlidersPanel::Render(Engine& engine, bool* open)
{
    if (!ImGui::Begin("Sliders", open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    std::vector<GlobalSlider>& sliders = engine.GetSliders();

    if (ImGui::Button("Add slider"))
        sliders.push_back(GlobalSlider{});

    ImGui::SameLine();
    ImGui::TextDisabled("read in Lua via slider(\"name\")");

    if (ImGui::BeginTable("##sliders", 5,
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody))
    {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("##min",  ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##val",  ImGuiTableColumnFlags_WidthStretch, 0.65f);
        ImGui::TableSetupColumn("##max",  ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##rm",   ImGuiTableColumnFlags_WidthFixed);

        for (int i = 0; i < (int)sliders.size(); ++i)
        {
            ImGui::PushID(i);
            GlobalSlider& s = sliders[i];

            ImGui::TableNextRow();

            ImGui::TableNextColumn();   // name
            char nameBuf[128];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.Name.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
                s.Name = nameBuf;

            ImGui::TableNextColumn();   // min
            ImGui::SetNextItemWidth(70.0f);
            ImGui::InputFloat("##min", &s.Min, 0.0f, 0.0f, "%.3f");

            ImGui::TableNextColumn();   // slider
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##val", &s.Value, s.Min, s.Max);

            ImGui::TableNextColumn();   // max
            ImGui::SetNextItemWidth(70.0f);
            ImGui::InputFloat("##max", &s.Max, 0.0f, 0.0f, "%.3f");

            ImGui::TableNextColumn();   // remove
            if (ImGui::SmallButton("x"))
            {
                sliders.erase(sliders.begin() + i);
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}
