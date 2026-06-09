#pragma once

#include "engine/Reflect.h"

#include <bgfx/bgfx.h>
#include <chrono>

// Custom BPM rewrites the frame beat (Context.IsBeat) for downstream effects.
// Three mutually exclusive modes (priority arbitrary > skip > invert):
//   arbitrary – fire a beat every 60000/ArbVal ms regardless of audio
//   skip      – pass every (SkipVal+1)-th incoming beat
//   invert    – fire when there is NO beat, suppress when there IS
// SkipFirst suppresses the first N incoming beats before any mode activates.
//
// All update logic lives here in Render(); the config UI only edits parameters
// and displays the before/after beat meters via the live-state accessors below.
struct CustomBpmConfig
{
    bool Arbitrary = true;
    bool Skip      = false;
    bool Invert    = false;
    int  ArbVal    = 120;      // BPM for arbitrary mode (6-300)
    int  SkipVal   = 1;        // pass every SkipVal+1 beats (1-16)
    int  SkipFirst = 0;        // suppress the first N incoming beats (0-64)
};

class CustomBpm : public ReflectedEffect<CustomBpmConfig>
{
public:
    void Init(bgfx::RendererType::Enum Renderer) override;
    void Render(const RenderContext& Ctx) override;
    void Destroy() override;

    // Beat-meter positions (0-7), updated each rendered frame. Read by the UI.
    int InMeterSeg()  const { return m_inSeg;  }
    int OutMeterSeg() const { return m_outSeg; }

protected:
    const std::vector<Field>& Fields() const override;
    std::string EffectName() const override { return "Custom BPM"; }
    void OnConfigChanged(const std::vector<std::string>& changed) override;

private:
    bgfx::ProgramHandle m_prog       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_texUniform = BGFX_INVALID_HANDLE;

    // Beat-modification runtime state (not serialized).
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_arbLast    = Clock::now();
    int               m_skipCount  = 0;
    int               m_beatCount  = 0;

    // Before/after beat meters: segment index (0-7) and bounce direction.
    int m_inSeg  = 0, m_inDir  = 1;
    int m_outSeg = 0, m_outDir = 1;
};
