#pragma once

// A list of RGB colors plus a runtime cycle position. The cycle interpolates
// between adjacent entries over 64 frames per pair — the integer-truncated
// fixed-point form from the original win32/JS color cycling — and is shared by
// every multi-color effect (Simple, SuperScope, Ring, RotatingStars,
// OscilloscopeStar, DotGrid). `Pos` is runtime-only and is not serialized.

#include <array>
#include <cstdint>
#include <vector>

struct ColorList
{
    std::vector<std::array<uint8_t, 3>> Entries = { { 255, 255, 255 } };
    int Pos = 0;   // runtime cycle position; not serialized

    bool   empty() const { return Entries.empty(); }
    size_t size()  const { return Entries.size(); }
    const std::array<uint8_t, 3>& operator[](size_t i) const { return Entries[i]; }

    void ResetCycle() { Pos = 0; }

    // Advances one step and returns the interpolated color as 0-255 ints
    // (white if the list is empty). A single-color list returns that exact color
    // at full brightness (the blend's max weight is 63/64, so interpolating a color
    // with itself would otherwise darken it ~1.6%).
    std::array<int, 3> StepU8()
    {
        if (Entries.empty()) return { 255, 255, 255 };
        if (Entries.size() == 1)
            return { Entries[0][0], Entries[0][1], Entries[0][2] };
        const int n    = (int)Entries.size();
        Pos = (Pos + 1) % (n * 64);
        const int seg  = Pos / 64;
        const int frac = Pos & 63;
        const auto& c1 = Entries[seg % n];
        const auto& c2 = Entries[(seg + 1) % n];
        return {
            (c1[0] * (63 - frac) + c2[0] * frac) / 64,
            (c1[1] * (63 - frac) + c2[1] * frac) / 64,
            (c1[2] * (63 - frac) + c2[2] * frac) / 64,
        };
    }

    // Same cycle, normalized to [0,1] floats.
    std::array<float, 3> StepF()
    {
        const auto c = StepU8();
        return { c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f };
    }
};
