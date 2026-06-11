#ifndef BILINEAR_COMPAT_SH
#define BILINEAR_COMPAT_SH

// 8-bit integer 2x2 bilinear blend — matches the original AVS C++
// blend_bilinear_2x2 exactly (fixed-point lerp with 8-bit fractional weights and
// >>8 truncation, on integer 0..255 channels). Use this instead of hardware
// bilinear when a warp/displacement effect needs to reproduce the original's
// look bit-for-bit; it fetches the 4 neighbour texels with texelFetch (nearest)
// and blends them in integer space, so the bound sampler's filter is ignored
// (bind it POINT).
//
// NOTE: keep this in sync with kBilinearCompatGlsl in src/engine/ShaderSnippets.h
// (the runtime-GLSL copy used by the dynamic Movement / Dynamic Movement effects).
//
// _tex : source sampler (bind with BGFX_SAMPLER_*_POINT — texelFetch ignores filter)
// _uv  : sample position in [0,1]
// _sz  : texture size in texels, e.g. textureSize(_tex, 0)
vec3 bilinearCompat(sampler2D _tex, vec2 _uv, ivec2 _sz)
{
    vec2  pos = _uv * vec2(_sz);
    ivec2 i0  = ivec2(floor(pos));
    ivec2 f8  = ivec2(fract(pos) * 256.0);
    ivec2 i1  = min(i0 + ivec2(1, 1), _sz - ivec2(1, 1));
    i0 = clamp(i0, ivec2(0, 0), _sz - ivec2(1, 1));
    ivec3 tl = ivec3(texelFetch(_tex, i0,                0).rgb * 255.0 + 0.5);
    ivec3 tr = ivec3(texelFetch(_tex, ivec2(i1.x, i0.y), 0).rgb * 255.0 + 0.5);
    ivec3 bl = ivec3(texelFetch(_tex, ivec2(i0.x, i1.y), 0).rgb * 255.0 + 0.5);
    ivec3 br = ivec3(texelFetch(_tex, i1,                0).rgb * 255.0 + 0.5);
    int fx = f8.x;
    int fy = f8.y;
    ivec3 top = (tl * (256 - fx) + tr * fx) >> 8;
    ivec3 bot = (bl * (256 - fx) + br * fx) >> 8;
    return vec3((top * (256 - fy) + bot * fy) >> 8) / 255.0;
}

#endif // BILINEAR_COMPAT_SH
