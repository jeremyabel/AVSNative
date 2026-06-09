$input v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_clearColor;

void main()
{
    gl_FragColor = vec4(u_clearColor.rgb, 1.0);
}
