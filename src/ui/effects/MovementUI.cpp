#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Movement.h"

#include <imgui.h>

#include <vector>
#include <string>

// Bespoke UI for Movement — plain ImGui drawn against the effect's Config, with
// conditional hiding (Blend) and BeginDisabled (On-Beat Toggle).
static void DrawMovementUI(Effect* base)
{
    auto* fx = static_cast<Movement*>(base);
    auto& c  = fx->ConfigRef();
    std::vector<std::string> changed;

    // --- Coordinate system (string select) ---
    const char* coordItems[] = { "polar", "cartesian" };
    int coordIdx = (c.Coordinates == "cartesian") ? 1 : 0;
    ImGui::TextUnformatted("Coordinates");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##coords", &coordIdx, coordItems, 2))
        fx->SetCoordinateSystem(coordItems[coordIdx]); // handles default-code swap + recompile

    // --- GLSL editor (shared helper owns the TextEditor instance) ---
    ImGui::TextUnformatted("GLSL Code");
    if (ConfigUi::CodeEditor("movement.code", c.Code, ConfigUi::Lang::Glsl))
        changed.push_back("code");
    if (std::string err = fx->GetScriptError("code"); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());

    ImGui::SeparatorText("Sampling");

    if (ImGui::Checkbox("Bilinear", &c.Bilinear)) changed.push_back("bilinear");
    if (ImGui::Checkbox("Wrap",     &c.Wrap))     changed.push_back("wrap");

    // Conditional: "Blend" is meaningless when Source Map is on — hide it entirely.
    if (!c.SourceMap)
        if (ImGui::Checkbox("Blend (50/50)", &c.Blend)) changed.push_back("blend");

    if (ImGui::Checkbox("Source Map", &c.SourceMap)) changed.push_back("sourceMap");

    // Conditional: On-Beat Toggle only relevant in polar mode — disable (greyed), keep visible.
    ImGui::BeginDisabled(c.Coordinates != "polar");
    if (ImGui::Checkbox("On-Beat Toggle", &c.OnBeatToggle)) changed.push_back("onBeatToggle");
    ImGui::EndDisabled();

    if (!changed.empty())
        fx->NotifyConfigChanged(changed);  // triggers Movement::Compile() when code changed
}

void RegisterMovementUI(ConfigUiRegistry& reg)
{
    reg.Register("Movement", &DrawMovementUI);
}
