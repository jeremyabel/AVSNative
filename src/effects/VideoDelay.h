#pragma once

#include "engine/Reflect.h"

#include <vector>

// Video Delay — a per-instance ring buffer of stored frames. Each frame outputs the
// oldest stored frame, overwrites that slot with the current input, and advances the
// write pointer, producing a fixed N-frame delay of the whole image.
//
// Like MultiDelay, ring slots are plain BGFX_TEXTURE_BLIT_DST textures (no framebuffers):
// the write path copies the input with bgfx::blit, the read path samples through the
// fullscreen blit shader. Ring size is capped at MAX_RING_SLOTS to bound VRAM.

struct VideoDelayConfig
{
    bool Enabled  = true;
    bool UseBeats = false;
    int  Delay    = 10;   // frames (UseBeats off) or beat multiplier (UseBeats on)
};

class VideoDelay : public ReflectedEffect<VideoDelayConfig>
{
public:
    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            Bool(&VideoDelayConfig::Enabled,  "enabled",  "Enabled"),
            Bool(&VideoDelayConfig::UseBeats, "usebeats", "Use Beats"),
            RangeI(&VideoDelayConfig::Delay,  "delay",    "Delay", 0, 200),
        };
        return f;
    }
    std::string EffectName() const override { return "Video Delay"; }

    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    void FreeRing();

    // Runtime state
    int m_frameDelay      = 10;
    int m_framesSinceBeat = 0;
    int m_writeIdx        = 0;
    int m_lastW           = 0;
    int m_lastH           = 0;

    std::vector<bgfx::TextureHandle> m_ring;  // BLIT_DST textures, no framebuffers

    // bgfx resources for the read (shader blit) pass.
    bgfx::ProgramHandle m_program  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_texUnif  = BGFX_INVALID_HANDLE;
};
