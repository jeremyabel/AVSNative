$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);

// (srcW, srcH, 0, 0)
uniform vec4 u_srcSize;

// Reduce pass: (min,max) texture -> half-res (min,max) by combining 2x2 blocks.
void main()
{
    ivec2 b  = ivec2(gl_FragCoord.xy) * 2;
    ivec2 sz = ivec2(int(u_srcSize.x) - 1, int(u_srcSize.y) - 1);
    vec4 s00 = texelFetch(s_input, clamp(b,               ivec2(0, 0), sz), 0);
    vec4 s10 = texelFetch(s_input, clamp(b + ivec2(1, 0), ivec2(0, 0), sz), 0);
    vec4 s01 = texelFetch(s_input, clamp(b + ivec2(0, 1), ivec2(0, 0), sz), 0);
    vec4 s11 = texelFetch(s_input, clamp(b + ivec2(1, 1), ivec2(0, 0), sz), 0);
    gl_FragColor = vec4(min(min(s00.r, s10.r), min(s01.r, s11.r)),
                        max(max(s00.g, s10.g), max(s01.g, s11.g)),
                        0.0, 1.0);
}
