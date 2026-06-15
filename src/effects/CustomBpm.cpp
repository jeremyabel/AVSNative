#include "effects/CustomBpm.h"

#include "engine/JsonUtil.h"

#include <algorithm>

static constexpr const char* NAME_EnableArbitraryMode = "arbitrary";
static constexpr const char* NAME_EnableSkip = "skip";
static constexpr const char* NAME_EnableInvert = "invert";
static constexpr const char* NAME_ArbitraryValue = "arbVal";
static constexpr const char* NAME_SkipValue = "skipVal";
static constexpr const char* NAME_SkipFirst = "skipfirst";

void CustomBpm::Init()
{
}

void CustomBpm::Destroy()
{
}

void CustomBpm::Render(const RenderContext& Context)
{
    const bool InBeat = Context.IsBeat();

    if (InBeat)
    {
        BeatCount++;
    }

    bool OutBeat = InBeat;
    if (SkipFirst != 0 && BeatCount <= SkipFirst)
    {
        OutBeat = false;
    }
    else if (Arbitrary)
    {
        const auto Now = Clock::now();
        const auto Period = std::chrono::milliseconds(60000 / std::max(1, ArbVal));
        
        if (Now - LastArbitraryBeat > Period) 
        { 
            LastArbitraryBeat = Now; 
            OutBeat = true; 
        }
        else
        {
            OutBeat = false;
        }
    }
    else if (Skip)
    {
        if (InBeat && ++SkipCount >= SkipVal + 1) 
        { 
            SkipCount = 0; 
            OutBeat = true; 
        }
        else
        {
            OutBeat = false;
        }
    }
    else if (Invert)
    {
        OutBeat = !InBeat;
    }

    Context.SetBeat(OutBeat);

    if (OutBeat)
    {
        OutCount++;
    }
}

nlohmann::json CustomBpm::Serialize() const
{
    return 
    {
        { NAME_EnableArbitraryMode, Arbitrary },
        { NAME_EnableSkip, Skip },
        { NAME_EnableInvert, Invert },
        { NAME_ArbitraryValue, ArbVal },
        { NAME_SkipValue, SkipVal },
        { NAME_SkipFirst, SkipFirst },
    };
}

void CustomBpm::Deserialize(const nlohmann::json& j)
{
    JsonUtil::ReadBool(j, NAME_EnableArbitraryMode, Arbitrary);
    JsonUtil::ReadBool(j, NAME_EnableSkip, Skip);
    JsonUtil::ReadBool(j, NAME_EnableInvert, Invert);
    JsonUtil::ReadInt(j, NAME_ArbitraryValue, ArbVal);
    JsonUtil::ReadInt(j, NAME_SkipValue, SkipVal);
    JsonUtil::ReadInt(j, NAME_SkipFirst, SkipFirst);
}