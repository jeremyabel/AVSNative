#pragma once

#include "engine/Effect.h"

class RotoBlitter : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Roto Blitter"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Snaps the zoom animation to the current ZoomScale. Called after Deserialize
    // and by the UI when the zoom slider changes.
    void ResetZoomAnim() { m_scaleFpos = (float)ZoomScale; }

public:

    int ZoomScale = 31; // 0-256; 31 = no zoom
    int ZoomScale2 = 31; // beat zoom target
    int RotDir = 31; // 0-64; 32 = no rotation, <32 one way, >32 other
    int BeatchSpeed = 0; // 0-8; rotation reversal smoothing
    bool Subpixel = true; // hardware bilinear (vs nearest)
    bool Compat = false; // 8-bit integer bilinear matching the win32 original
    bool Blend = false;
    bool Beatch = false; // reverse rotation on beat
    bool BeatchScale = false; // snap zoom on beat

private:

    float m_rotRev = 1.0f; // target: 1 or -1
    float m_rotRevPos = 1.0f; // smoothed rotRev
    float m_scaleFpos = 31.0f; // animates toward ZoomScale

    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TransformUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ResolutionUniform = BGFX_INVALID_HANDLE;
};
