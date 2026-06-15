#include "VideoDelay.h"

#include "engine/JsonUtil.h"

#include "engine/FBOManager.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"

#include <algorithm>
#include <cstring>

static constexpr const char* NAME_UseBeats = "usebeats";
static constexpr const char* NAME_Delay = "delay";

// Cap ring size to bound VRAM (the original's 400-frame max is impractical on GPU).
static constexpr int MAX_RING_SLOTS = 64;

static bgfx::TextureHandle MakeSlot(uint16_t w, uint16_t h)
{
    // Zero-initialised so the first `delay` frames (before the ring fills) output black
    // rather than uninitialised GPU memory. BLIT_DST so the write path can copy the
    // input in via bgfx::blit; sampler clamp so the read path can sample it.
    const bgfx::Memory* mem = bgfx::alloc((uint32_t)w * h * 4);
    std::memset(mem->data, 0, mem->size);
    return bgfx::createTexture2D(
        w, h, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);
}

void VideoDelay::Init()
{
    bgfx::ShaderHandle VertShader = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FragShader = bgfx::createShader(bgfx::copy(fs_blit_spv, sizeof(fs_blit_spv)));
    Program = bgfx::createProgram(VertShader, FragShader, true);

    TexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    m_frameDelay = Delay;
}

void VideoDelay::FreeRing()
{
    for (bgfx::TextureHandle t : m_ring)
        if (bgfx::isValid(t)) bgfx::destroy(t);
    m_ring.clear();
    m_writeIdx = 0;
}

void VideoDelay::ApplyDelayChange()
{
    m_frameDelay      = 0;
    m_framesSinceBeat = 0;

    // Beats mode multiplies the delay each beat, so it uses a tighter cap.
    const int hi = UseBeats ? 16 : 200;
    Delay = std::clamp(Delay, 0, hi);
    if (!UseBeats) m_frameDelay = Delay;
}

void VideoDelay::Render(const RenderContext& Context)
{
    const uint16_t w = (uint16_t)Context.Width;
    const uint16_t h = (uint16_t)Context.Height;

    // Resolve the active frame delay.
    if (UseBeats)
    {
        if (Context.IsBeat())
        {
            m_frameDelay = std::min(m_framesSinceBeat * Delay, 400);
            m_framesSinceBeat = 0;
        }
        m_framesSinceBeat++;
    }
    else
    {
        m_frameDelay = Delay;
    }

    if (m_frameDelay == 0)
        return;

    const int slots = std::min(m_frameDelay, MAX_RING_SLOTS);

    // Reallocate the ring on resize.
    if (w != m_lastW || h != m_lastH)
    {
        FreeRing();
        m_lastW = w;
        m_lastH = h;
    }

    // Grow the ring lazily to cover the needed slots.
    while ((int)m_ring.size() < slots)
    {
        m_ring.push_back(MakeSlot(w, h));
    }

    // Keep the write index in bounds if the slot count shrank.
    m_writeIdx %= slots;

    const bgfx::TextureHandle slot = m_ring[m_writeIdx];

    // Read pass (viewId): output the oldest stored frame → output FBO. EffectChain
    // already bound this view to the next ping-pong slot.
    bgfx::setTexture(0, TexUniform, slot);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, Context.QuadVB);
    bgfx::submit(Context.ViewId, Program);

    // Write pass (viewId+1): overwrite that slot with the current input. A higher view
    // ID guarantees this transfer runs *after* the read above, so we never read the
    // frame we just wrote.
    const uint8_t writeView = Context.ViewId + 1;
    bgfx::setViewClear(writeView, BGFX_CLEAR_NONE);
    bgfx::blit(writeView, slot, 0, 0, 0, 0, Context.InputTexture, 0, 0, 0, 0, w, h);

    m_writeIdx = (m_writeIdx + 1) % slots;
    Context.FboManager->Swap();
}

void VideoDelay::Destroy()
{
    FreeRing();

    if (bgfx::isValid(TexUniform)) 
        bgfx::destroy(TexUniform);

    if (bgfx::isValid(Program)) 
        bgfx::destroy(Program);

    TexUniform = BGFX_INVALID_HANDLE;
    Program = BGFX_INVALID_HANDLE;
}

nlohmann::json VideoDelay::Serialize() const
{
    return 
    {
        { NAME_UseBeats, UseBeats },
        { NAME_Delay, Delay },
    };
}

void VideoDelay::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadBool(j, NAME_UseBeats, UseBeats);
    JsonUtil::ReadInt (j, NAME_Delay, Delay);

    ApplyDelayChange();
}
