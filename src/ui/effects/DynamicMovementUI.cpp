#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/DynamicMovement.h"

#include <imgui.h>

static void DrawDynamicMovementUI(Effect* base)
{
    auto* fx = static_cast<DynamicMovement*>(base);
    
    if (ImGui::TreeNodeEx("Init", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dmove.initCode", fx->InitCode, ConfigUi::Lang::Lua))
        fx->ApplyInitCodeChange();
        ConfigUi::ScriptError(fx, DynamicMovement::kInitCode);
        ImGui::TreePop();
    }
    
    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Beat", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dmove.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
            fx->ApplyBeatCodeChange();
        ConfigUi::ScriptError(fx, DynamicMovement::kBeatCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Frame", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dmove.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
            fx->ApplyFrameCodeChange();
        ConfigUi::ScriptError(fx, DynamicMovement::kFrameCode);
        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Pixel", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (ConfigUi::CodeEditor("dmove.pixelCode", fx->PixelCode, ConfigUi::Lang::Glsl))
            fx->ApplyPixelCodeChange();
        ConfigUi::ScriptError(fx, DynamicMovement::kPixelCode);
        ImGui::TreePop();
    }

    ImGui::Checkbox("Cartesian Coords", &fx->RectCoords);
    ImGui::Checkbox("Wrap", &fx->Wrap);
    ImGui::Checkbox("Blend", &fx->Blend);
    ImGui::Checkbox("Bilinear", &fx->Bilinear);

    ImGui::BeginDisabled(!fx->Bilinear);
    ImGui::Checkbox("Bilinear (precise)", &fx->BilinearCompat);
    ImGui::EndDisabled();

    ImGui::Checkbox("No Movement", &fx->NoMove);
    ImGui::Checkbox("Use Grid", &fx->UseGrid);

    ImGui::BeginDisabled(!fx->UseGrid);
    ImGui::TextUnformatted("Grid Width");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##gridW", &fx->GridW, 1, 256);

    ImGui::TextUnformatted("Grid Height");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##gridH", &fx->GridH, 1, 256);
    ImGui::EndDisabled();

    static const char* kBuffers[] = { "Current", "Buffer 1", "Buffer 2", "Buffer 3",
                                      "Buffer 4", "Buffer 5", "Buffer 6", "Buffer 7",
                                      "Buffer 8" };
    ImGui::TextUnformatted("Source Buffer");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##bufferN", &fx->BufferN, kBuffers, IM_ARRAYSIZE(kBuffers));
}

void RegisterDynamicMovementUI(ConfigUiRegistry& reg)
{
    reg.Register("Dynamic Movement", &DrawDynamicMovementUI);
}
