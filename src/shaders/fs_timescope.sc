$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input,  0);   // current framebuffer (persists frame-to-frame)
SAMPLER2D(s_column, 1);   // 1×h scope column for this frame (RGB = color * magnitude)

// x = position (column to draw this frame), y = width (px),
// z = blend mode (0/3 replace, 1 additive, 2 50/50)
uniform vec4 u_tsParams;

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

    vec3 scope = texture2D(s_column, vec2(0.5, uv.y)).rgb;

    int mode = int(u_tsParams.z + 0.5);
    vec3 outc;
    if      (mode == 1) outc = min(base + scope, vec3_splat(1.0));  // additive
    else if (mode == 2) outc = (base + scope) * 0.5;               // 50/50
    else                outc = scope;                             // replace / default

    gl_FragColor = vec4(outc, 1.0);
}
