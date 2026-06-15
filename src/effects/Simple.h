#pragma once

#include "engine/Effect.h"
#include "engine/ColorList.h"

struct NVGcontext;
struct NVGLUframebuffer;

class Simple : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Simple"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

public:

    int Mode = 2; // 0=solid analyzer, 1=line analyzer, 2=line scope, 3=solid scope
    int Channel = 0; // 0=L, 1=R, 2=mix
    int Position = 1; // 0=top, 1=center, 2=bottom
    bool AntialiasingEnabled = false;
    ColorList Colors;

private:

    void EnsureOverlay(int Width, int Height);
    void EnsureNvgContext();
    void DestroyOverlay();

    // NanoVG
    NVGcontext* NvgContext = nullptr;
    NVGLUframebuffer* OverlayFBO = nullptr;
    bool NvgEdgeAA = false;
    int OverlayW = 0;
    int OverlayH = 0;

    // bgfx composite pass
    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle OverlayUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ParamsUniform = BGFX_INVALID_HANDLE;
};
