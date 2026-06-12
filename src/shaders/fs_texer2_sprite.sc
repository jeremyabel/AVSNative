$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_sprite, 0);  // the loaded image (RGBA8)

uniform vec4 u_color;   // rgb = per-particle colorize multiplier (white when off)
uniform vec4 u_style;   // x = style (0 premult, 1 mix-white, 2 adjust), y = param

// Outputs the particle's texel shaped for the per-mode GPU blend selected on the CPU.
// `cov` (the image's alpha) is the coverage, so transparent texels are no-ops under
// every mode's blend state.
void main()
{
    vec4  t   = texture2D(s_sprite, v_texcoord0.xy);
    vec3  col = t.rgb * u_color.rgb;
    float cov = t.a;

    int style = int(u_style.x + 0.5);
    if (style == 1)
    {
        // Multiply / Minimum: lerp toward white by coverage so transparent texels
        // leave the destination unchanged (dst*1 / min(dst,1)).
        gl_FragColor = vec4(mix(vec3_splat(1.0), col, cov), cov);
    }
    else if (style == 2)
    {
        // Adjustable / 50-50: premultiply by (param * coverage).
        float w = u_style.y * cov;
        gl_FragColor = vec4(col * w, w);
    }
    else
    {
        // Premultiply by coverage (Replace/Additive/Max/Sub/XOR-approx).
        gl_FragColor = vec4(col * cov, cov);
    }
}
