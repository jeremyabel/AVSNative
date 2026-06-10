#include "effects/CustomBpm.h"

#include <algorithm>

const std::vector<Field>& CustomBpm::Fields() const
{
    static const std::vector<Field> kFields = {
        ::Bool(&CustomBpmConfig::Arbitrary, "arbitrary", "Arbitrary BPM"),
        ::Bool(&CustomBpmConfig::Skip,      "skip",      "Skip Beats"),
        ::Bool(&CustomBpmConfig::Invert,    "invert",    "Invert Beat"),
        RangeI(&CustomBpmConfig::ArbVal,    "arbVal",    "Arbitrary BPM", 6, 300),
        RangeI(&CustomBpmConfig::SkipVal,   "skipVal",   "Skip",          1, 16),
        RangeI(&CustomBpmConfig::SkipFirst, "skipfirst", "Skip First N",  0, 64),
    };
    return kFields;
}

void CustomBpm::OnConfigChanged(const std::vector<std::string>& changed)
{
    // The three modes are mutually exclusive; turning one on clears the others.
    const auto has = [&](const char* k) {
        return std::find(changed.begin(), changed.end(), k) != changed.end();
    };
    if      (has("arbitrary") && Cfg.Arbitrary) { Cfg.Skip = false; Cfg.Invert = false; }
    else if (has("skip")      && Cfg.Skip)      { Cfg.Arbitrary = false; Cfg.Invert = false; }
    else if (has("invert")    && Cfg.Invert)    { Cfg.Arbitrary = false; Cfg.Skip = false; }
}

void CustomBpm::Init()
{
}

void CustomBpm::Render(const RenderContext& Ctx)
{
    // ── Beat modification (mirrors the reference render) ──
    // Reads the beat as it arrives at this point in the chain, then rewrites it
    // so downstream effects see the modified beat.
    const bool inBeat = Ctx.IsBeat();

    if (inBeat)
    {
        m_beatCount++;
        m_inSeg += m_inDir;
        if      (m_inSeg >= 7) { m_inSeg = 7; m_inDir = -1; }
        else if (m_inSeg <= 0) { m_inSeg = 0; m_inDir =  1; }
    }

    bool outBeat = inBeat;
    if (Cfg.SkipFirst != 0 && m_beatCount <= Cfg.SkipFirst)
    {
        outBeat = false;
    }
    else if (Cfg.Arbitrary)
    {
        const auto now = Clock::now();
        const auto period = std::chrono::milliseconds(60000 / std::max(1, Cfg.ArbVal));
        if (now - m_arbLast > period) { m_arbLast = now; outBeat = true; }
        else                          outBeat = false;
    }
    else if (Cfg.Skip)
    {
        if (inBeat && ++m_skipCount >= Cfg.SkipVal + 1) { m_skipCount = 0; outBeat = true; }
        else                                            outBeat = false;
    }
    else if (Cfg.Invert)
    {
        outBeat = !inBeat;
    }

    Ctx.SetBeat(outBeat);

    if (outBeat)
    {
        m_outSeg += m_outDir;
        if      (m_outSeg >= 7) { m_outSeg = 7; m_outDir = -1; }
        else if (m_outSeg <= 0) { m_outSeg = 0; m_outDir =  1; }
    }

    // No image output and no swap: EffectChain runs control-only effects in place.
}

void CustomBpm::Destroy()
{
}
