#pragma once

#include "engine/Reflect.h"

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

struct TimescopeConfig
{
    int Channel = 2;   // 0 = Left, 1 = Right, 2 = Center (avg) — spectrum
    std::array<uint8_t, 3> Color = { 255, 255, 255 };
    int Blend = 3;     // 0 = Replace, 1 = Additive, 2 = 50/50, 3 = Default (= Replace)
    int Bands = 576;   // spectrum bins spread across the column height (16..576)
};

class Timescope : public ReflectedEffect<TimescopeConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&TimescopeConfig::Channel, "channel", "Source", { "Left", "Right", "Center" }),
            Color(&TimescopeConfig::Color, "color", "Color"),
            SelectI(&TimescopeConfig::Blend, "blend", "Blend", { "Replace", "Additive", "50/50", "Default" }),
            RangeI(&TimescopeConfig::Bands, "bands", "Bands", 16, 576),
        };
        return f;
    }
    std::string EffectName() const override { return "Timescope"; }

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
