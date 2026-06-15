$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);   // current framebuffer (persists frame-to-frame)
SAMPLER2D(s_audio, 1);   // 576x1 audio texture: R=spec_L, G=spec_R (raw 0..255 / 255)

// x = position (column to draw this frame), y = width (px),
// z = blend mode (0/3 replace, 1 additive, 2 50/50), w = height (px)
uniform vec4 u_tsParams;
// rgb = color (0..255), w = bands (spectrum bins spread across column height)
uniform vec4 u_tsColor;
// x = channel (0=Left, 1=Right, 2=Center)
uniform vec4 u_tsParams2;

void main()
{
    vec2 uv   = v_texcoord0.xy;
    vec3 base = texture2D(s_input, uv).rgb;

    int col = int(floor(uv.x * u_tsParams.y));
    int pos = int(u_tsParams.x + 0.5);

    // Every column except the current one passes the framebuffer through unchanged;
    // those columns hold scope data drawn on previous frames, so the spectrogram scrolls.
    if (col != pos)
    {
        gl_FragColor = vec4(base, 1.0);
        return;
    }

    int h     = int(u_tsParams.w + 0.5);
    int bands = int(u_tsColor.w + 0.5);
    int chan  = int(u_tsParams2.x + 0.5);

    // Row along the column height → spectrum bin, with the original's integer indexing.
    int row = int(floor(uv.y * float(h)));
    int bin = clamp((row * bands) / h, 0, 575);

    // Recover the raw 0..255 spectrum magnitudes (bit-exact to the CPU path).
    vec2  spec = texelFetch(s_audio, ivec2(bin, 0), 0).rg;
    float vL   = floor(spec.r * 255.0 + 0.5);
    float vR   = floor(spec.g * 255.0 + 0.5);

    float val;
    if      (chan == 2) val = floor(vL / 2.0) + floor(vR / 2.0);  // center
    else if (chan == 0) val = vL;
    else                val = vR;

    // color × magnitude / 256 (original fixed-point scaling), in integer space.
    vec3 scope = floor(u_tsColor.rgb * val / 256.0) / 255.0;

    int mode = int(u_tsParams.z + 0.5);
    vec3 outc;
    if      (mode == 1) outc = min(base + scope, vec3_splat(1.0));  // additive
    else if (mode == 2) outc = (base + scope) * 0.5;               // 50/50
    else                outc = scope;                             // replace / default

    gl_FragColor = vec4(outc, 1.0);
}
