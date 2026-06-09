#pragma once

#include "engine/Reflect.h"

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

struct MultiDelayConfig
{
    int Mode         = 0;   // 0=Disabled, 1=Write to buffer, 2=Read from buffer
    int ActiveBuffer = 0;   // 0..5
};

class MultiDelay : public ReflectedEffect<MultiDelayConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    // The per-buffer delay/unit settings live in shared global state, so they are
    // serialized on top of the generic per-instance config (mode/activebuffer).
    nlohmann::json GetConfig() const override;
    void SetConfig(const nlohmann::json& Config) override;

protected:
    const std::vector<Field>& Fields() const override
    {
        static const std::vector<Field> f = {
            SelectI(&MultiDelayConfig::Mode, "mode", "Mode",
                    { "Disabled", "Write to buffer", "Read from buffer" }),
            SelectI(&MultiDelayConfig::ActiveBuffer, "activebuffer", "Buffer",
                    { "Buffer 1", "Buffer 2", "Buffer 3", "Buffer 4", "Buffer 5", "Buffer 6" }),
        };
        return f;
    }
    std::string EffectName() const override { return "Multi Delay"; }

private:
    bool                m_inited     = false;
    bgfx::ProgramHandle BlitProgram  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle TexUniform   = BGFX_INVALID_HANDLE;
};
