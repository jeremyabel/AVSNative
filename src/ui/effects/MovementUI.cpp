#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/Movement.h"

#include <imgui.h>

#include <string>

// Bespoke UI for Movement — plain ImGui editing the effect's members directly,
// with conditional hiding (Blend) and BeginDisabled (On-Beat Toggle).
static void DrawMovementUI(Effect* base)
{
    auto* fx = static_cast<Movement*>(base);

    const char* coordItems[] = { "polar", "cartesian" };
    int coordIdx = (fx->Coordinates == "cartesian") ? 1 : 0;
    ImGui::TextUnformatted("Coordinates");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##coords", &coordIdx, coordItems, 2))
        fx->SetCoordinateSystem(coordItems[coordIdx]); // handles default-code swap + recompile

    if (ImGui::TreeNodeEx("GLSL Code", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("movement.code", fx->Code, ConfigUi::Lang::Glsl))
            fx->Compile();
        ConfigUi::ScriptError(fx, Movement::kCode);
        ImGui::TreePop();
    }

    ImGui::SeparatorText("Sampling");

    ImGui::Checkbox("Bilinear", &fx->Bilinear);

    // Precise = original AVS 8-bit integer bilinear; only meaningful with Bilinear on.
    ImGui::BeginDisabled(!fx->Bilinear);
    ImGui::Checkbox("Bilinear (precise)", &fx->Compat);
    ImGui::EndDisabled();

    ImGui::Checkbox("Wrap", &fx->Wrap);

    // Conditional: "Blend" is meaningless when Source Map is on — hide it entirely.
    if (!fx->SourceMap)
        ImGui::Checkbox("Blend (50/50)", &fx->Blend);

    ImGui::Checkbox("Source Map", &fx->SourceMap);

    // Conditional: On-Beat Toggle only relevant in polar mode — disable (greyed), keep visible.
    ImGui::BeginDisabled(fx->Coordinates != "polar");
    ImGui::Checkbox("On-Beat Toggle", &fx->OnBeatToggle);
    ImGui::EndDisabled();
}

void RegisterMovementUI(ConfigUiRegistry& reg)
{
    reg.Register("Movement", &DrawMovementUI);
}
