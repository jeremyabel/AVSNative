$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// xyz=dot color (0-1), w=blend (0=replace 1=additive 2=50/50 3=default/screen)
uniform vec4 u_dgColor;
// x=spacing, y=sx, z=sy (all integer grid params stored as float)
uniform vec4 u_dgGrid;
// x=canvas width, y=canvas height
uniform vec4 u_dgSize;

void main()
{
    vec2 uv = v_texcoord0.xy;
    vec3 base = texture2D(s_texColor, uv).rgb;

    int spacing = int(u_dgGrid.x);
    int sx = int(u_dgGrid.y);
    int sy = int(u_dgGrid.z);
    int blend = int(u_dgColor.w);

    int px = int(uv.x * u_dgSize.x);
    int py = int(uv.y * u_dgSize.y);

    bool isDot = (spacing > 0)
              && ((px - sx) % spacing == 0)
              && ((py - sy) % spacing == 0);

    if (!isDot)
    {
        gl_FragColor = vec4(base, 1.0);
        return;
    }

    vec3 dot = u_dgColor.rgb;
    vec3 result;
    if      (blend == 1) result = min(base + dot, vec3(1.0, 1.0, 1.0));
    else if (blend == 2) result = (base + dot) * 0.5;
    else if (blend == 3) result = base + dot - base * dot;
    else                 result = dot;

    gl_FragColor = vec4(result, 1.0);
}
