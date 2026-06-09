$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x = dir: 0 = ×2 brighter (saturating), 1 = ×½ darker, 2 = no change
uniform vec4 u_fbParams;

void main()
{
    vec3 c = texture2D(s_texColor, v_texcoord0.xy).rgb;
    int dir = int(u_fbParams.x + 0.5);
    if      (dir == 0) c = min(c * 2.0, vec3_splat(1.0));
    else if (dir == 1) c = c * 0.5;
    gl_FragColor = vec4(c, 1.0);
}
