#pragma once

#include "engine/Effect.h"

#include <array>
#include <cstdint>
#include <vector>

// Timescope — a scrolling spectrogram. Each frame draws one vertical column from the
// spectrum (vertical axis = frequency bin, brightness = magnitude), advancing the
// column position horizontally so the image scrolls. Faithful to the original
// vis_avs e_timescope.cpp (NOT the AVSWeb port, which drew threshold bars/waveforms).
//
// Every pixel of the column is written (color × magnitude/256); persistence between
// frames — the engine reuses the previous frame's framebuffer as input — is what
// accumulates the scrolling history, mirroring the original's in-place framebuffer.

class Timescope : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int Channel = 2;   // 0 = Left, 1 = Right, 2 = Center (avg) — spectrum
    std::array<uint8_t, 3> Color = { 255, 255, 255 };
    int Blend = 3;     // 0 = Replace, 1 = Additive, 2 = 50/50, 3 = Default (= Replace)
    int Bands = 576;   // spectrum bins spread across the column height (16..576)

    static constexpr const char* kChannel = "channel";
    static constexpr const char* kColor   = "color";
    static constexpr const char* kBlend   = "blend";
    static constexpr const char* kBands   = "bands";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Timescope"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    void EnsureScope(int w, int h);

    int m_position = 0;
    int m_scopeW = 0, m_scopeH = 0;
    std::vector<uint8_t> m_column;            // 1×h RGBA8 scope column for the current frame

    bgfx::TextureHandle m_columnTex = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_program   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uInput    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uColumn   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uParams   = BGFX_INVALID_HANDLE;
};
