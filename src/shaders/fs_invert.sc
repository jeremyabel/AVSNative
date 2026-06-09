$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

void main()
{
    vec3 c = texture2D(s_texColor, v_texcoord0.xy).rgb;
    gl_FragColor = vec4(1.0 - c, 1.0);
}
