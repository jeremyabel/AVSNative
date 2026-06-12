$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);

// rgb = strobe color, w = on (1) / off (0)
uniform vec4 u_strobeColor;

void main()
{
    if (u_strobeColor.w > 0.5)
        gl_FragColor = vec4(u_strobeColor.rgb, 1.0);          // flat color
    else
        gl_FragColor = vec4(texture2D(s_input, v_texcoord0.xy).rgb, 1.0);  // passthrough
}
