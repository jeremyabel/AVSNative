#pragma once

// Reusable GLSL snippets injected into runtime-compiled shaders (the dynamic
// Movement / Dynamic Movement effects, which build their fragment source as
// strings and compile via glslang). Static .sc effects get the same helpers
// from the matching include in src/shaders/ (e.g. bilinear_compat.sh).

namespace avs {

// 8-bit integer 2x2 bilinear blend — matches the original AVS C++
// blend_bilinear_2x2 exactly (fixed-point lerp with 8-bit fractional weights and
// >>8 truncation, on integer 0..255 channels). Bind the source POINT — this does
// its own nearest texelFetch of the 4 neighbours and blends them in integer space.
//
// Keep in sync with src/shaders/bilinear_compat.sh (the .sc-include copy).
//   tex : source sampler
//   uv  : sample position in [0,1]
//   sz  : texture size in texels, e.g. textureSize(tex, 0)
inline constexpr const char* kBilinearCompatGlsl = R"(
vec3 bilinearCompat(sampler2D tex, vec2 uv, ivec2 sz) {
    vec2  pos = uv * vec2(sz);
    ivec2 i0  = ivec2(floor(pos));
    ivec2 f8  = ivec2(fract(pos) * 256.0);
    ivec2 i1  = min(i0 + ivec2(1), sz - ivec2(1));
    i0 = clamp(i0, ivec2(0), sz - ivec2(1));
    ivec3 tl = ivec3(texelFetch(tex, i0,                0).rgb * 255.0 + 0.5);
    ivec3 tr = ivec3(texelFetch(tex, ivec2(i1.x, i0.y), 0).rgb * 255.0 + 0.5);
    ivec3 bl = ivec3(texelFetch(tex, ivec2(i0.x, i1.y), 0).rgb * 255.0 + 0.5);
    ivec3 br = ivec3(texelFetch(tex, i1,                0).rgb * 255.0 + 0.5);
    int fx = f8.x;
    int fy = f8.y;
    ivec3 top = (tl * (256 - fx) + tr * fx) >> 8;
    ivec3 bot = (bl * (256 - fx) + br * fx) >> 8;
    return vec3((top * (256 - fy) + bot * fy) >> 8) / 255.0;
}
)";

} // namespace avs
