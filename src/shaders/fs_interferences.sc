$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// Packed UV-space offsets: each vec4 = (off[N].x, off[N].y, off[N+1].x, off[N+1].y)
uniform vec4 u_ifOffsets0;  // points 0, 1
uniform vec4 u_ifOffsets1;  // points 2, 3
uniform vec4 u_ifOffsets2;  // points 4, 5
uniform vec4 u_ifOffsets3;  // points 6, 7
// x=nPoints, y=alpha(0-255), z=rgb(0/1), w=outBlend(0=replace 1=additive 2=average)
uniform vec4 u_ifParams;

// Fetch a source texel as integer 0..255 channels. Off-screen samples contribute
// black, matching the win32 original's bounds check (xp>=0 && xp<w, yoffs!=-1).
// The texture is point-sampled and offsets are texel-aligned, so this reads exactly
// one source pixel — no bilinear blend, which is what keeps the feedback bounded.
vec3 fetch255(vec2 p)
{
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
        return vec3(0.0, 0.0, 0.0);
    return floor(texture2D(s_texColor, p).rgb * 255.0 + 0.5);
}

// Integer alpha multiply with truncation: floor(v * alpha / 255), matching the
// win32 lut_u8_multiply[alpha][v] table. The +1e-4 nudges exact-integer results
// that float division would otherwise land just below (e.g. 254.9999 -> 255).
vec3 amul(vec3 v, float alpha)
{
    return floor(v * alpha / 255.0 + 1e-4);
}

void main()
{
    vec2 uv   = v_texcoord0.xy;
    vec3 orig = texture2D(s_texColor, uv).rgb;

    int   nPoints  = int(u_ifParams.x);
    float alpha    = u_ifParams.y;   // 0..255
    int   rgb      = int(u_ifParams.z);
    int   outBlend = int(u_ifParams.w);

    if (nPoints == 0)
    {
        gl_FragColor = vec4(orig, 1.0);
        return;
    }

    // Accumulate in integer 0..255 pixel space.
    vec3 acc = vec3(0.0, 0.0, 0.0);

    if (rgb == 1)
    {
        // Each of the first 3 offsets contributes one channel (B, G, R).
        acc.b = amul(fetch255(uv - u_ifOffsets0.xy), alpha).b;
        acc.g = amul(fetch255(uv - u_ifOffsets0.zw), alpha).g;
        acc.r = amul(fetch255(uv - u_ifOffsets1.xy), alpha).r;
        if (nPoints == 6)
        {
            acc.b = min(acc.b + amul(fetch255(uv - u_ifOffsets1.zw), alpha).b, 255.0);
            acc.g = min(acc.g + amul(fetch255(uv - u_ifOffsets2.xy), alpha).g, 255.0);
            acc.r = min(acc.r + amul(fetch255(uv - u_ifOffsets2.zw), alpha).r, 255.0);
        }
    }
    else
    {
        if (nPoints >= 1) acc += amul(fetch255(uv - u_ifOffsets0.xy), alpha);
        if (nPoints >= 2) acc += amul(fetch255(uv - u_ifOffsets0.zw), alpha);
        if (nPoints >= 3) acc += amul(fetch255(uv - u_ifOffsets1.xy), alpha);
        if (nPoints >= 4) acc += amul(fetch255(uv - u_ifOffsets1.zw), alpha);
        if (nPoints >= 5) acc += amul(fetch255(uv - u_ifOffsets2.xy), alpha);
        if (nPoints >= 6) acc += amul(fetch255(uv - u_ifOffsets2.zw), alpha);
        if (nPoints >= 7) acc += amul(fetch255(uv - u_ifOffsets3.xy), alpha);
        if (nPoints >= 8) acc += amul(fetch255(uv - u_ifOffsets3.zw), alpha);
        acc = min(acc, vec3(255.0, 255.0, 255.0));
    }

    vec3 col = acc / 255.0;

    vec3 result;
    if (outBlend == 1)
        result = clamp(orig + col, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
    else if (outBlend == 2)
        result = (orig + col) * 0.5;
    else
        result = col;

    gl_FragColor = vec4(result, 1.0);
}
