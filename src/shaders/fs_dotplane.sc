$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input,   0);
SAMPLER2D(s_overlay, 1);

// Screen blend (b + s - b*s) where the overlay has a drawn dot; pass-through otherwise.
// Matches dot-plane.js's composite shader.
void main()
{
    vec2 uv   = v_texcoord0.xy;
    vec4 base = texture2D(s_input,   uv);
    vec4 dot  = texture2D(s_overlay, uv);

    if (dot.a < 0.5)
    {
        gl_FragColor = vec4(base.rgb, 1.0);
        return;
    }

    vec3 b = base.rgb;
    vec3 s = dot.rgb;
    gl_FragColor = vec4(b + s - b * s, 1.0);
}
