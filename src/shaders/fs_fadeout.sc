$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// xyz = target color, w = blend amount (0 = no fade, 1 = full replace)
uniform vec4 u_fadeParams;

void main()
{
    vec4 Color = texture2D(s_texColor, v_texcoord0.xy);
    gl_FragColor = mix(Color, vec4(u_fadeParams.xyz, 1.0), u_fadeParams.w);
}
