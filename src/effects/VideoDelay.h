#pragma once

#include "engine/Effect.h"

#include <vector>

// Video Delay — a per-instance ring buffer of stored frames. Each frame outputs the
// oldest stored frame, overwrites that slot with the current input, and advances the
// write pointer, producing a fixed N-frame delay of the whole image.
//
// Like MultiDelay, ring slots are plain BGFX_TEXTURE_BLIT_DST textures (no framebuffers):
// the write path copies the input with bgfx::blit, the read path samples through the
// fullscreen blit shader. Ring size is capped at MAX_RING_SLOTS to bound VRAM.

class VideoDelay : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    bool Enabled  = true;
    bool UseBeats = false;
    int  Delay    = 10;   // frames (UseBeats off, 0–200) or beat multiplier (UseBeats on, 0–16)

    static constexpr const char* kEnabled  = "enabled";
    static constexpr const char* kUseBeats = "usebeats";
    static constexpr const char* kDelay    = "delay";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Video Delay"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Re-derives the runtime frame delay after UseBeats/Delay change (beats mode
    // multiplies the delay per beat, so it uses a tighter cap). Called after
    // Deserialize and by the UI.
    void ApplyDelayChange();

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
