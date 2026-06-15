$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);

// (srcW, srcH, 0, 0)
uniform vec4 u_srcSize;

// Init pass: color input -> per-pixel (min,max) across a 2x2 block.
// Output R = global min of all channels in block, G = global max.
void main()
{
    ivec2 b  = ivec2(gl_FragCoord.xy) * 2;
    ivec2 sz = ivec2(int(u_srcSize.x) - 1, int(u_srcSize.y) - 1);
    vec3 s00 = texelFetch(s_input, clamp(b,               ivec2(0, 0), sz), 0).rgb;
    vec3 s10 = texelFetch(s_input, clamp(b + ivec2(1, 0), ivec2(0, 0), sz), 0).rgb;
    vec3 s01 = texelFetch(s_input, clamp(b + ivec2(0, 1), ivec2(0, 0), sz), 0).rgb;
    vec3 s11 = texelFetch(s_input, clamp(b + ivec2(1, 1), ivec2(0, 0), sz), 0).rgb;
    vec3 mn3 = min(min(s00, s10), min(s01, s11));
    vec3 mx3 = max(max(s00, s10), max(s01, s11));
    gl_FragColor = vec4(min(mn3.r, min(mn3.g, mn3.b)),
                        max(mx3.r, max(mx3.g, mx3.b)),
                        0.0, 1.0);
}
