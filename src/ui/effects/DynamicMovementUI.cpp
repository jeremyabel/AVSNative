#include "ui/ConfigUiRegistry.h"
#include "ui/ConfigUi.h"

#include "effects/DynamicMovement.h"

#include <imgui.h>

#include <string>

static void ScriptError(Effect* fx, const char* param)
{
    if (std::string err = fx->GetScriptError(param); !err.empty())
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
}

static void DrawDynamicMovementUI(Effect* base)
{
    auto* fx = static_cast<DynamicMovement*>(base);

    ImGui::TextUnformatted("Pixel");
    if (ConfigUi::CodeEditor("dmove.pixelCode", fx->PixelCode, ConfigUi::Lang::Glsl))
        fx->ApplyPixelCodeChange();
    ScriptError(fx, DynamicMovement::kPixelCode);

    ImGui::TextUnformatted("Frame");
    if (ConfigUi::CodeEditor("dmove.frameCode", fx->FrameCode, ConfigUi::Lang::Lua))
        fx->ApplyFrameCodeChange();
    ScriptError(fx, DynamicMovement::kFrameCode);

    ImGui::TextUnformatted("Beat");
    if (ConfigUi::CodeEditor("dmove.beatCode", fx->BeatCode, ConfigUi::Lang::Lua))
        fx->ApplyBeatCodeChange();
    ScriptError(fx, DynamicMovement::kBeatCode);

    ImGui::TextUnformatted("Init");
    if (ConfigUi::CodeEditor("dmove.initCode", fx->InitCode, ConfigUi::Lang::Lua))
        fx->ApplyInitCodeChange();
    ScriptError(fx, DynamicMovement::kInitCode);

    ImGui::Checkbox("Cartesian Coords", &fx->RectCoords);
    ImGui::Checkbox("Wrap", &fx->Wrap);
    ImGui::Checkbox("Blend", &fx->Blend);
    ImGui::Checkbox("Bilinear", &fx->Bilinear);

    ImGui::BeginDisabled(!fx->Bilinear);
    ImGui::Checkbox("Bilinear (precise)", &fx->BilinearCompat);
    ImGui::EndDisabled();

    ImGui::Checkbox("No Movement", &fx->NoMove);
    ImGui::Checkbox("Show UV (debug)", &fx->ShowUV);
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
