$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// xyz=color(0-1), w=blend(0=replace 1=average)
uniform vec4 u_obcColor;

void main()
{
    vec3 color = u_obcColor.rgb;

    if (int(u_obcColor.w) == 1)
    {
        vec3 orig = texture2D(s_texColor, v_texcoord0.xy).rgb;
        gl_FragColor = vec4((orig + color) * 0.5, 1.0);
    }
    else
    {
        gl_FragColor = vec4(color, 1.0);
    }
}
