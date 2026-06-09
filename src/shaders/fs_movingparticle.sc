$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texInput, 0);

uniform vec4 u_particle;    // xy=center UV, z=radius_px, w=blend_mode
uniform vec4 u_color;       // xyz=rgb (0..1), w=unused
uniform vec4 u_resolution;  // xy=width, height

void main()
{
    vec2 uv  = v_texcoord0.xy;
    vec4 src = texture2D(s_texInput, uv);

    vec2  center   = u_particle.xy;
    float radiusPx = u_particle.z;
    float mode     = u_particle.w;

    // SDF circle: distance in pixel space for aspect-ratio-correct shape
    vec2  fragPx   = uv     * u_resolution.xy;
    vec2  centerPx = center * u_resolution.xy;
    float dist     = length(fragPx - centerPx) - radiusPx;

    // 1-pixel anti-aliased edge
    float alpha = 1.0 - smoothstep(-0.5, 0.5, dist);

    vec3 col = u_color.rgb;
    vec4 result;

    if (mode < 0.5)
    {
        // Replace
        result = mix(src, vec4(col, 1.0), alpha);
    }
    else if (mode < 1.5)
    {
        // Additive
        result = vec4(min(src.rgb + col * alpha, vec3(1.0, 1.0, 1.0)), src.a);
    }
    else if (mode < 2.5)
    {
        // 50/50
        result = mix(src, mix(src, vec4(col, 1.0), alpha), 0.5);
    }
    else
    {
        // Default (same as additive)
        result = vec4(min(src.rgb + col * alpha, vec3(1.0, 1.0, 1.0)), src.a);
    }

    gl_FragColor = result;
}
