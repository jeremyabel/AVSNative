$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x=borderW, y=borderH, z=canvasW, w=canvasH  (all in pixels)
uniform vec4 u_borderParams;
// xyz=border colour (normalised 0-1), w=unused
uniform vec4 u_borderColor;

void main()
{
    float bw = u_borderParams.x;
    float bh = u_borderParams.y;
    float cw = u_borderParams.z;
    float ch = u_borderParams.w;

    vec2 fc = gl_FragCoord.xy;
    bool inBorder = fc.x < bw || fc.x >= cw - bw
                 || fc.y < bh || fc.y >= ch - bh;

    if (inBorder)
        gl_FragColor = vec4(u_borderColor.rgb, 1.0);
    else
        gl_FragColor = vec4(texture2D(s_texColor, v_texcoord0.xy).rgb, 1.0);
}
