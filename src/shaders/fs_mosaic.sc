$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x = curSize (blocks across; >=100 means no mosaic), y = resW, z = resH, w = blend mode
uniform vec4 u_mosaicParams;

void main()
{
    float n    = u_mosaicParams.x;
    vec2  res  = u_mosaicParams.yz;
    int   mode = int(u_mosaicParams.w + 0.5);

    vec2 uv   = v_texcoord0.xy;
    vec3 orig = texture2D(s_texColor, uv).rgb;

    // size == 100 (or more) → full resolution, passthrough (matches the original's
    // `if (cur_size < 100)` guard).
    if (n >= 100.0)
    {
        gl_FragColor = vec4(orig, 1.0);
        return;
    }

    // Each of the n×n blocks samples the source at the block's top-left pixel.
    vec2 blockIdx = floor(uv * n);
    vec2 srcPx    = floor(blockIdx * res / n) + 0.5;
    vec3 mos      = texture2D(s_texColor, srcPx / res).rgb;

    // Blend the mosaic color with the original pixel.
    vec3 outc;
    if      (mode == 1) outc = min(orig + mos, vec3_splat(1.0));  // additive
    else if (mode == 2) outc = (orig + mos) * 0.5;               // 50/50
    else                outc = mos;                             // replace

    gl_FragColor = vec4(outc, 1.0);
}
