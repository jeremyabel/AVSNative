#pragma once

#include "engine/Effect.h"

// Multi Delay — delay-line buffers shared across all MultiDelay instances in a preset.
//
// Typical use: one instance (mode=Write) stores the current frame into one of 6 shared
// ring buffers; a later instance (mode=Read) pulls a time-delayed copy back out. The 6
// buffers and their per-buffer delay/unit settings are GLOBAL (shared by every instance),
// mirroring the original AVS which kept the buffers in global C++ state. Only `Mode` and
// `ActiveBuffer` are per-instance.
//
// Texture allocation: each ring entry is a plain BGFX_TEXTURE_BLIT_DST texture (NOT a
// framebuffer). Writes copy the input via bgfx::blit; reads sample the stored texture
// through the fullscreen blit shader. This keeps framebuffer usage at zero for the rings
// (bgfx caps out at 128 framebuffers but 4096 textures), at the cost of up to
// 6 * MAX_RING full-resolution textures of VRAM worst-case.

class MultiDelay : public Effect
{
public:
    // ── Config (serialized; edited directly by the UI) ─────────────────────────
    int Mode         = 0;   // 0=Disabled, 1=Write to buffer, 2=Read from buffer
    int ActiveBuffer = 0;   // 0..5

    static constexpr const char* kMode           = "mode";
    static constexpr const char* kActiveBuffer   = "activebuffer";
    // Shared per-buffer settings serialize as usebeats0..5 / delay0..5.
    static constexpr const char* kUseBeatsPrefix = "usebeats";
    static constexpr const char* kDelayPrefix    = "delay";

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Multi Delay"; }
    // The per-buffer delay/unit settings live in shared global state, so they are
    // serialized on top of the per-instance config (mode/activebuffer).
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    // Accessors for the shared (global) per-buffer settings, used by the UI.
    // SetBufferDelay applies the frame delay immediately in frame mode; beat mode
    // picks the new value up on the next beat.
    static bool GetBufferUseBeats(int i);
    static void SetBufferUseBeats(int i, bool useBeats);
    static int  GetBufferDelay(int i);
    static void SetBufferDelay(int i, int delay);

private:
    bool                m_inited     = false;
    bgfx::ProgramHandle BlitProgram  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
};
