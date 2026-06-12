#include "MultiDelay.h"

#include "engine/FBOManager.h"
#include "engine/JsonUtil.h"

#include "generated/spirv/vs_fullscreen.sc.bin.h"
#include "generated/spirv/fs_blit.sc.bin.h"

#include <algorithm>
#include <cstring>
#include <vector>

// VRAM cap: a single ring slot never exceeds MAX_RING textures. Worst case across all
// 6 slots is 6 * MAX_RING full-resolution RGBA8 textures (~6 * 32 * w*h*4 bytes).
static constexpr int MAX_RING = 32;

// ── Shared global state ─────────────────────────────────────────────────────────
// One instance per process (one bgfx context), mirroring the JS module-level singleton.
namespace
{
struct RingSlot
{
    std::vector<bgfx::TextureHandle> Ring;   // BLIT_DST textures, no framebuffers
    int InIdx  = 0;                          // write position
    int OutIdx = 0;                          // read position (delay frames behind InIdx)
    int Size   = 0;
};

struct MultiDelayShared
{
    int      InstanceCount  = 0;
    int64_t  LastFrame      = -1;            // last Frame we ran per-frame housekeeping for
    int      FramesSinceBeat = 0;
    int      FramesPerBeat   = 0;
    uint16_t LastW = 0, LastH = 0;

    bool UseBeats[6]    = {};
    int  Delays[6]      = {};                // user value (frames, or beat count)
    int  FrameDelays[6] = {};                // actual delay in frames (= delay+1 in frame mode)

    RingSlot Slots[6];
};

MultiDelayShared& Shared()
{
    static MultiDelayShared s;
    return s;
}

bgfx::TextureHandle MakeRingTexture(uint16_t w, uint16_t h)
{
    // Initialise to black so reads during the first `delay` frames (before the ring
    // fills) show black rather than uninitialised GPU memory. BLIT_DST so the write
    // path can copy the input frame in with bgfx::blit; sampler clamp so the read path
    // can sample it through the blit shader.
    const bgfx::Memory* mem = bgfx::alloc((uint32_t)w * h * 4);
    std::memset(mem->data, 0, mem->size);
    return bgfx::createTexture2D(
        w, h, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);
}

void FreeSlot(RingSlot& slot)
{
    for (bgfx::TextureHandle t : slot.Ring)
        if (bgfx::isValid(t)) bgfx::destroy(t);
    slot.Ring.clear();
    slot.Size = 0;
    slot.InIdx = 0;
    slot.OutIdx = 0;
}

void FreeAllSlots(MultiDelayShared& S)
{
    for (RingSlot& slot : S.Slots)
        FreeSlot(slot);
}

// Resize (or allocate) one slot to match the required frame delay.
void EnsureSlot(MultiDelayShared& S, int i, uint16_t w, uint16_t h)
{
    const int fd = S.FrameDelays[i];
    RingSlot& slot = S.Slots[i];

    if (fd <= 1)
    {
        if (slot.Size > 0) FreeSlot(slot);
        return;
    }

    const int needed = std::min(fd, MAX_RING);
    if (slot.Size == needed) return;

    // Size changed: rebuild. A brief transition glitch is acceptable (matches the
    // original's "allocate new memory" path without the complex copy logic).
    FreeSlot(slot);
    slot.Ring.reserve(needed);
    for (int j = 0; j < needed; j++)
        slot.Ring.push_back(MakeRingTexture(w, h));
    slot.Size   = needed;
    slot.OutIdx = 0;
    slot.InIdx  = needed - 1;   // distance = framedelay-1 (capped)
}
} // namespace

// ── Effect ──────────────────────────────────────────────────────────────────────

void MultiDelay::Init()
{
    bgfx::ShaderHandle VS = bgfx::createShader(bgfx::copy(vs_fullscreen_spv, sizeof(vs_fullscreen_spv)));
    bgfx::ShaderHandle FS = bgfx::createShader(bgfx::copy(fs_blit_spv,       sizeof(fs_blit_spv)));
    BlitProgram = bgfx::createProgram(VS, FS, true);
    TexUniform  = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    if (!m_inited)
    {
        Shared().InstanceCount++;
        m_inited = true;
    }
}

void MultiDelay::Render(const RenderContext& Context)
{
    MultiDelayShared& S = Shared();
    const uint16_t w = (uint16_t)Context.Width;
    const uint16_t h = (uint16_t)Context.Height;

    // ── Per-frame housekeeping (runs once, on the first instance to see a new frame) ──
    // Frame-keyed rather than the JS renderid/numinstances scheme: this stays correct
    // even when some MultiDelay instances are disabled (and so never render this frame).
    if (Context.Frame != S.LastFrame)
    {
        // Advance ring pointers for the previous frame's writes. Doing this at the start
        // of a new frame is equivalent to advancing at the end of the previous one: each
        // frame still writes a fresh InIdx and reads the oldest OutIdx, gap held constant.
        if (S.LastFrame >= 0)
        {
            for (RingSlot& slot : S.Slots)
                if (slot.Size > 0)
                {
                    slot.InIdx  = (slot.InIdx  + 1) % slot.Size;
                    slot.OutIdx = (slot.OutIdx + 1) % slot.Size;
                }
        }

        // Resize → drop every ring (rebuilt below).
        if (w != S.LastW || h != S.LastH)
        {
            FreeAllSlots(S);
            S.LastW = w;
            S.LastH = h;
        }

        // Beat-based delay update.
        if (Context.IsBeat())
        {
            S.FramesPerBeat = S.FramesSinceBeat;
            for (int i = 0; i < 6; i++)
                if (S.UseBeats[i]) S.FrameDelays[i] = S.FramesPerBeat + 1;
            S.FramesSinceBeat = 0;
        }
        S.FramesSinceBeat++;

        // Ensure every active slot has the right ring size.
        for (int i = 0; i < 6; i++)
            EnsureSlot(S, i, w, h);

        S.LastFrame = Context.Frame;
    }

    // ── This instance's action ────────────────────────────────────────────────────
    const int ab = std::clamp(ActiveBuffer, 0, 5);
    RingSlot& slot = S.Slots[ab];
    if (Mode == 0 || S.FrameDelays[ab] <= 1 || slot.Size == 0)
        return;   // disabled / no delay / unallocated → pass-through (no swap)

    if (Mode == 1)
    {
        // Write: copy the current frame into the ring at InIdx. The pipeline image is
        // unchanged (no swap), so downstream effects still see the input. EffectChain
        // pre-bound this view to the next ping-pong FBO with a clear — cancel it since
        // we only issue a transfer, not a draw.
        bgfx::setViewClear(Context.ViewId, BGFX_CLEAR_NONE);
        bgfx::blit(Context.ViewId,
                   slot.Ring[slot.InIdx], 0, 0, 0, 0,
                   Context.InputTexture,  0, 0, 0, 0,
                   w, h);
    }
    else
    {
        // Read: replace the pipeline image with the delayed frame at OutIdx. The view is
        // already bound to OutputFBO (the next ping-pong slot) by EffectChain.
        bgfx::setTexture(0, TexUniform, slot.Ring[slot.OutIdx]);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::setVertexBuffer(0, Context.QuadVB);
        bgfx::submit(Context.ViewId, BlitProgram);
        Context.FboManager->Swap();
    }
}

void MultiDelay::Destroy()
{
    if (bgfx::isValid(TexUniform))  bgfx::destroy(TexUniform);
    if (bgfx::isValid(BlitProgram)) bgfx::destroy(BlitProgram);
    TexUniform  = BGFX_INVALID_HANDLE;
    BlitProgram = BGFX_INVALID_HANDLE;

    if (m_inited)
    {
        m_inited = false;
        MultiDelayShared& S = Shared();
        if (--S.InstanceCount <= 0)
        {
            // Last instance gone: free the shared rings (bgfx is still live here — chains
            // are cleared before bgfx::shutdown) and reset shared state.
            FreeAllSlots(S);
            S.InstanceCount   = 0;
            S.LastFrame       = -1;
            S.FramesSinceBeat = 0;
            S.FramesPerBeat   = 0;
            S.LastW = S.LastH = 0;
            for (int i = 0; i < 6; i++)
            {
                S.UseBeats[i]    = false;
                S.Delays[i]      = 0;
                S.FrameDelays[i] = 0;
            }
        }
    }
}

// ── Shared per-buffer setting accessors (used by the UI) ─────────────────────────

bool MultiDelay::GetBufferUseBeats(int i)
{
    return (i >= 0 && i < 6) ? Shared().UseBeats[i] : false;
}

void MultiDelay::SetBufferUseBeats(int i, bool useBeats)
{
    if (i < 0 || i >= 6) return;
    Shared().UseBeats[i] = useBeats;
}

int MultiDelay::GetBufferDelay(int i)
{
    return (i >= 0 && i < 6) ? Shared().Delays[i] : 0;
}

void MultiDelay::SetBufferDelay(int i, int delay)
{
    if (i < 0 || i >= 6) return;
    MultiDelayShared& S = Shared();
    S.Delays[i] = std::max(0, delay);
    // Frame mode: apply immediately. Beat mode waits for the next beat.
    if (!S.UseBeats[i])
        S.FrameDelays[i] = S.Delays[i] + 1;
}

// ── Serialization (shared per-buffer settings layered on the per-instance config) ─

nlohmann::json MultiDelay::Serialize() const
{
    nlohmann::json j = {
        { kMode,         Mode         },
        { kActiveBuffer, ActiveBuffer },
    };
    for (int i = 0; i < 6; i++)
    {
        j[kUseBeatsPrefix + std::to_string(i)] = GetBufferUseBeats(i) ? 1 : 0;
        j[kDelayPrefix    + std::to_string(i)] = GetBufferDelay(i);
    }
    return j;
}

void MultiDelay::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadInt(j, kMode,         Mode);
    JsonUtil::ReadInt(j, kActiveBuffer, ActiveBuffer);

    for (int i = 0; i < 6; i++)
    {
        const std::string ubKey = kUseBeatsPrefix + std::to_string(i);
        const std::string dKey  = kDelayPrefix    + std::to_string(i);
        if (j.contains(ubKey) && j.at(ubKey).is_number())
            SetBufferUseBeats(i, j.at(ubKey).get<int>() != 0);
        if (j.contains(dKey) && j.at(dKey).is_number())
            SetBufferDelay(i, j.at(dKey).get<int>());
    }
}
