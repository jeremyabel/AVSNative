$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// Packed UV-space offsets: each vec4 = (off[N].x, off[N].y, off[N+1].x, off[N+1].y)
uniform vec4 u_ifOffsets0;  // points 0, 1
uniform vec4 u_ifOffsets1;  // points 2, 3
uniform vec4 u_ifOffsets2;  // points 4, 5
uniform vec4 u_ifOffsets3;  // points 6, 7
// x=nPoints, y=alpha(0-1), z=rgb(0/1), w=outBlend(0=replace 1=additive 2=average)
uniform vec4 u_ifParams;

void main()
{
    vec2 uv   = v_texcoord0.xy;
    vec3 orig = texture2D(s_texColor, uv).rgb;

    int   nPoints  = int(u_ifParams.x);
    float alpha    = u_ifParams.y;
    int   rgb      = int(u_ifParams.z);
    int   outBlend = int(u_ifParams.w);

    vec2 lo = vec2(0.0, 0.0);
    vec2 hi = vec2(1.0, 1.0);

    vec3 col = vec3(0.0, 0.0, 0.0);

    if (nPoints == 0)
    {
        gl_FragColor = vec4(orig, 1.0);
        return;
    }

    if (rgb == 1)
    {
        // Each of the first 3 offsets contributes one channel (B, G, R).
        vec3 s0 = texture2D(s_texColor, clamp(uv - u_ifOffsets0.xy, lo, hi)).rgb;
        vec3 s1 = texture2D(s_texColor, clamp(uv - u_ifOffsets0.zw, lo, hi)).rgb;
        vec3 s2 = texture2D(s_texColor, clamp(uv - u_ifOffsets1.xy, lo, hi)).rgb;
        col.b = s0.b * alpha;
        col.g = s1.g * alpha;
        col.r = s2.r * alpha;
        if (nPoints == 6)
        {
            vec3 s3 = texture2D(s_texColor, clamp(uv - u_ifOffsets1.zw, lo, hi)).rgb;
            vec3 s4 = texture2D(s_texColor, clamp(uv - u_ifOffsets2.xy, lo, hi)).rgb;
            vec3 s5 = texture2D(s_texColor, clamp(uv - u_ifOffsets2.zw, lo, hi)).rgb;
            col.b = clamp(col.b + s3.b * alpha, 0.0, 1.0);
            col.g = clamp(col.g + s4.g * alpha, 0.0, 1.0);
            col.r = clamp(col.r + s5.r * alpha, 0.0, 1.0);
        }
    }
    else
    {
        if (nPoints >= 1) col += texture2D(s_texColor, clamp(uv - u_ifOffsets0.xy, lo, hi)).rgb * alpha;
        if (nPoints >= 2) col += texture2D(s_texColor, clamp(uv - u_ifOffsets0.zw, lo, hi)).rgb * alpha;
        if (nPoints >= 3) col += texture2D(s_texColor, clamp(uv - u_ifOffsets1.xy, lo, hi)).rgb * alpha;
        if (nPoints >= 4) col += texture2D(s_texColor, clamp(uv - u_ifOffsets1.zw, lo, hi)).rgb * alpha;
        if (nPoints >= 5) col += texture2D(s_texColor, clamp(uv - u_ifOffsets2.xy, lo, hi)).rgb * alpha;
        if (nPoints >= 6) col += texture2D(s_texColor, clamp(uv - u_ifOffsets2.zw, lo, hi)).rgb * alpha;
        if (nPoints >= 7) col += texture2D(s_texColor, clamp(uv - u_ifOffsets3.xy, lo, hi)).rgb * alpha;
        if (nPoints >= 8) col += texture2D(s_texColor, clamp(uv - u_ifOffsets3.zw, lo, hi)).rgb * alpha;
        col = clamp(col, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
    }

    vec3 result;
    if (outBlend == 1)
        result = clamp(orig + col, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
    else if (outBlend == 2)
        result = (orig + col) * 0.5;
    else
        result = col;

    gl_FragColor = vec4(result, 1.0);
}
