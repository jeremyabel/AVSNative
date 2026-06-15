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

public:

    bool UseBeats = false;
    int Delay = 10; 

private:

    void FreeRing();

    // Runtime state
    int m_frameDelay = 10;
    int m_framesSinceBeat = 0;
    int m_writeIdx = 0;
    int m_lastW = 0;
    int m_lastH = 0;

    std::vector<bgfx::TextureHandle> m_ring;  // BLIT_DST textures, no framebuffers

    // bgfx resources for the read (shader blit) pass.
    bgfx::ProgramHandle Program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform = BGFX_INVALID_HANDLE;
};
