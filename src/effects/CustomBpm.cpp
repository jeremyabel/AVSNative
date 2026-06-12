#include "effects/CustomBpm.h"

#include "engine/JsonUtil.h"

#include <algorithm>

nlohmann::json CustomBpm::Serialize() const
{
    return {
        { kArbitrary, Arbitrary },
        { kSkip,      Skip      },
        { kInvert,    Invert    },
        { kArbVal,    ArbVal    },
        { kSkipVal,   SkipVal   },
        { kSkipFirst, SkipFirst },
    };
}

void CustomBpm::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadBool(j, kArbitrary, Arbitrary);
    JsonUtil::ReadBool(j, kSkip,      Skip);
    JsonUtil::ReadBool(j, kInvert,    Invert);
    JsonUtil::ReadInt (j, kArbVal,    ArbVal);
    JsonUtil::ReadInt (j, kSkipVal,   SkipVal);
    JsonUtil::ReadInt (j, kSkipFirst, SkipFirst);
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
    if (SkipFirst != 0 && m_beatCount <= SkipFirst)
    {
        outBeat = false;
    }
    else if (Arbitrary)
    {
        const auto now = Clock::now();
        const auto period = std::chrono::milliseconds(60000 / std::max(1, ArbVal));
        if (now - m_arbLast > period) { m_arbLast = now; outBeat = true; }
        else                          outBeat = false;
    }
    else if (Skip)
    {
        if (inBeat && ++m_skipCount >= SkipVal + 1) { m_skipCount = 0; outBeat = true; }
        else                                            outBeat = false;
    }
    else if (Invert)
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
