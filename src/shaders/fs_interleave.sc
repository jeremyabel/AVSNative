$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// xyz=color(0-1), w=outBlend(0=replace 1=additive 2=average)
uniform vec4 u_ilColor;
// x=tx, y=ty, z=width, w=height (all as floats, cast to int in shader)
uniform vec4 u_ilGrid;

void main()
{
    vec2 uv   = v_texcoord0.xy;
    vec3 orig = texture2D(s_texColor, uv).rgb;

    int tx = int(u_ilGrid.x);
    int ty = int(u_ilGrid.y);
    int w  = int(u_ilGrid.z);
    int h  = int(u_ilGrid.w);

    // Shortcut: both zero means nothing to draw
    if (tx == 0 && ty == 0)
    {
        gl_FragColor = vec4(orig, 1.0);
        return;
    }

    // Pixel coordinates, top-left origin (UV 0,0 = top-left in bgfx/Vulkan)
    int px = int(uv.x * float(w));
    int py = int(uv.y * float(h));

    bool applyColor = false;

    bool isPureColorRow = false;
    bool inGridRow      = false;

    if (ty == 0)
    {
        inGridRow = true;
    }
    else
    {
        int yp_start = (h % ty) / 2;
        int yPhase   = (py + yp_start + 1) % (2 * ty);
        isPureColorRow = yPhase < ty;
        inGridRow      = !isPureColorRow;
    }

    if (isPureColorRow)
    {
        applyColor = true;
    }
    else if (inGridRow && tx > 0)
    {
        int xos    = (w % tx) / 2;
        int xPhase = (px + xos) % (2 * tx);
        applyColor = xPhase < tx;
    }

    if (!applyColor)
    {
        gl_FragColor = vec4(orig, 1.0);
        return;
    }

    vec3 color   = u_ilColor.rgb;
    int  outBlend = int(u_ilColor.w);

    vec3 result;
    if (outBlend == 1)
        result = clamp(orig + color, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
    else if (outBlend == 2)
        result = (orig + color) * 0.5;
    else
        result = color;

    gl_FragColor = vec4(result, 1.0);
}
