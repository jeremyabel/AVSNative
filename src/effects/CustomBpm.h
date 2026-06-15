#pragma once

#include "engine/Effect.h"

#include <chrono>

// Custom BPM rewrites the frame beat (Context.IsBeat) for downstream effects.
class CustomBpm : public Effect
{
public:

    void Init() override;
    void Render(const RenderContext& Context) override;
    void Destroy() override;

    std::string Name() const override { return "Custom BPM"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;
    uint8_t ExpectedViewCount() const override { return 0; } // Control only, no rendering

    int GetInBeatCount() const { return BeatCount; }
    int GetOutBeatCount() const { return OutCount; }

public:

    bool Arbitrary = true;
    bool Skip = false;
    bool Invert = false;
    int ArbVal = 120;
    int SkipVal = 1;
    int SkipFirst = 0;

private:

    using Clock = std::chrono::steady_clock;
    Clock::time_point LastArbitraryBeat = Clock::now();

    int SkipCount = 0;
    int BeatCount = 0;
    int OutCount = 0;
};
