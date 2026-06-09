$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x = mode bitmask: bit0 = flipX, bit1 = flipY
uniform vec4 u_mirrorParams;

void main()
{
    int mode = int(u_mirrorParams.x + 0.5);
    vec2 uv = v_texcoord0.xy;
    if ((mode & 1) != 0) uv.x = 1.0 - uv.x;
    if ((mode & 2) != 0) uv.y = 1.0 - uv.y;
    gl_FragColor = texture2D(s_texColor, uv);
}
