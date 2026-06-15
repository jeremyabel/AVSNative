#pragma once

#include "engine/Effect.h"
#include "engine/ColorList.h"

#include <bgfx/bgfx.h>

struct NVGcontext;
struct NVGLUframebuffer;

class OscilloscopeStar : public Effect
{
public:

    static constexpr const char* NAME_Colors       = "colors";
    static constexpr const char* NAME_AudioChannel = "audioChannel";
    static constexpr const char* NAME_Position     = "position";
    static constexpr const char* NAME_Size         = "size";
    static constexpr const char* NAME_Rotation     = "rotation";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Oscilloscope Star"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    ColorList Colors;
    int AudioChannel = 2;   // 0=Left, 1=Right, 2=Center
    int Position     = 2;   // 0=Left, 1=Right, 2=Center
    int Size         = 8;   // 0..32  — fraction of screen: Size/32
    int Rotation     = 0;   // -16..16 (speed): +0.01*Rotation rad/frame

private:

    void EnsureOverlay(int W, int H);
    void DestroyOverlay();

    NVGcontext* NvgContext = nullptr;
    NVGLUframebuffer* OverlayFBO = nullptr;
    int OverlayWidth = 0, OverlayHeight = 0;

    float CurrentRotation = 0.0f;

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle OverlayUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
