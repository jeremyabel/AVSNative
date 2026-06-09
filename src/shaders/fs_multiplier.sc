$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x=mode(0-7)
uniform vec4 u_mulParams;

void main()
{
    vec3 c   = texture2D(s_texColor, v_texcoord0.xy).rgb;
    int  mode = int(u_mulParams.x);

    if (mode == 0)
    {
        // Inv: any non-black → white
        c = ((c.r + c.g + c.b) > 0.0) ? vec3(1.0, 1.0, 1.0) : vec3(0.0, 0.0, 0.0);
    }
    else if (mode == 1)
    {
        c = min(c * 8.0, vec3(1.0, 1.0, 1.0));
    }
    else if (mode == 2)
    {
        c = min(c * 4.0, vec3(1.0, 1.0, 1.0));
    }
    else if (mode == 3)
    {
        c = min(c * 2.0, vec3(1.0, 1.0, 1.0));
    }
    else if (mode == 4)
    {
        // ×½: floor integer right-shift 1
        c = floor(c * 255.0 / 2.0) / 255.0;
    }
    else if (mode == 5)
    {
        // ×¼: floor integer right-shift 2
        c = floor(c * 255.0 / 4.0) / 255.0;
    }
    else if (mode == 6)
    {
        // ×⅛: floor integer right-shift 3
        c = floor(c * 255.0 / 8.0) / 255.0;
    }
    else
    {
        // XS: only exact white survives
        c = (c.r > (254.5 / 255.0) && c.g > (254.5 / 255.0) && c.b > (254.5 / 255.0))
            ? vec3(1.0, 1.0, 1.0) : vec3(0.0, 0.0, 0.0);
    }

    gl_FragColor = vec4(c, 1.0);
}
