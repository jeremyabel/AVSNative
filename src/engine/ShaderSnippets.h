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
// Keep the blend math in sync with src/shaders/bilinear_compat.sh (the .sc-include
// copy). The signature differs deliberately: ShaderCompiler::SeparateCombinedSamplers
// rewrites our combined `sampler2D` declarations into bgfx's separated
// texture2D + sampler form, and GLSL forbids passing a *constructed* combined
// sampler through a function parameter ("sampler constructor must appear at point
// of use"). So this takes the texture and sampler separately and reconstructs the
// combined sampler at each texelFetch. Call it via the BILINEAR_COMPAT(name, …)
// macro, which maps a sampler name to its generated `_name_t` / `_name_s` pair.
//   name : a sampler declared as `uniform sampler2D name;`
//   uv   : sample position in [0,1]
//   sz   : texture size in texels, e.g. textureSize(name, 0)
inline constexpr const char* kBilinearCompatGlsl = R"(
vec3 bilinearCompat(texture2D tex, sampler smp, vec2 uv, ivec2 sz) {
    vec2  pos = uv * vec2(sz);
    ivec2 i0  = ivec2(floor(pos));
    ivec2 f8  = ivec2(fract(pos) * 256.0);
    ivec2 i1  = min(i0 + ivec2(1), sz - ivec2(1));
    i0 = clamp(i0, ivec2(0), sz - ivec2(1));
    ivec3 tl = ivec3(texelFetch(sampler2D(tex, smp), i0,                0).rgb * 255.0 + 0.5);
    ivec3 tr = ivec3(texelFetch(sampler2D(tex, smp), ivec2(i1.x, i0.y), 0).rgb * 255.0 + 0.5);
    ivec3 bl = ivec3(texelFetch(sampler2D(tex, smp), ivec2(i0.x, i1.y), 0).rgb * 255.0 + 0.5);
    ivec3 br = ivec3(texelFetch(sampler2D(tex, smp), i1,                0).rgb * 255.0 + 0.5);
    int fx = f8.x;
    int fy = f8.y;
    ivec3 top = (tl * (256 - fx) + tr * fx) >> 8;
    ivec3 bot = (bl * (256 - fx) + br * fx) >> 8;
    return vec3((top * (256 - fy) + bot * fy) >> 8) / 255.0;
}
#define BILINEAR_COMPAT(name, uv, sz) bilinearCompat(_##name##_t, _##name##_s, uv, sz)
)";

} // namespace avs
