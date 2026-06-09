$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x = mode (0=RGB, 1=RBG, 2=GRB, 3=GBR, 4=BRG, 5=BGR)
uniform vec4 u_csParams;

void main()
{
    vec3 c = texture2D(s_texColor, v_texcoord0.xy).rgb;
    vec3 r;
    int mode = int(u_csParams.x);
    if      (mode == 1) r = c.rbg;
    else if (mode == 2) r = c.grb;
    else if (mode == 3) r = c.gbr;
    else if (mode == 4) r = c.brg;
    else if (mode == 5) r = c.bgr;
    else                r = c.rgb;
    gl_FragColor = vec4(r, 1.0);
}
