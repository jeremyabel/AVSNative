#pragma once

// Shared math constants.
//
// These use `inline constexpr` (C++17): a single, externally-linked definition that is
// ODR-merged across all translation units. That makes them safe under unity/jumbo
// builds, where the old per-file `static constexpr float kPi = ...` copies could either
// collide (two identical names in one chunk) or silently leak a definition from one
// file into a neighbour that never declared it.
namespace avs
{
inline constexpr float Pi    = 3.14159265358979323846f;
inline constexpr float TwoPi = 2.0f * Pi;
inline constexpr float Phi   = 1.61803398874989484820f;   // golden ratio
}
