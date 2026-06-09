$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x = 2^levels (precomputed)
uniform vec4 u_crParams;

void main()
{
    float levels = u_crParams.x;
    vec3 c = texture2D(s_texColor, v_texcoord0.xy).rgb;
    gl_FragColor = vec4(floor(c * levels) / levels, 1.0);
}
