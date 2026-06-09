$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);

uniform vec4 u_mult;     // xyz = per-channel multipliers
uniform vec4 u_params;   // x = blend (0 replace, 1 additive, 2 50/50), y = exclude(0/1), z = distance
uniform vec4 u_exclude;  // xyz = exclude color (normalized)

void main()
{
    vec3 c = texture2D(s_input, v_texcoord0.xy).rgb;

    // Color exclusion: pixels within `distance` of the exclude color on every channel
    // are left untouched (matches the original in_range() skip).
    if (u_params.y > 0.5)
    {
        vec3 d = abs(c - u_exclude.xyz);
        if (d.x <= u_params.z && d.y <= u_params.z && d.z <= u_params.z)
        {
            gl_FragColor = vec4(c, 1.0);
            return;
        }
    }

    vec3 adj = clamp(c * u_mult.xyz, vec3_splat(0.0), vec3_splat(1.0));

    int mode = int(u_params.x + 0.5);
    vec3 outc;
    if      (mode == 1) outc = min(adj + c, vec3_splat(1.0));  // additive
    else if (mode == 2) outc = (adj + c) * 0.5;               // 50/50
    else                outc = adj;                           // replace

    gl_FragColor = vec4(outc, 1.0);
}
