$input v_texcoord0

#include <bgfx_shader.sh>
#include "bilinear_compat.sh"

SAMPLER2D(s_texColor, 0);

// x=zoom, y=angle (accumulated radians), z=centerX, w=centerY
uniform vec4 u_blitParams;

// x=compat(0/1)
uniform vec4 u_blitFlags;

void main()
{
    float zoom = u_blitParams.x;
    float angle = u_blitParams.y;
    vec2 center = u_blitParams.zw;
    bool compat = u_blitFlags.x > 0.5;

    vec2 uv = v_texcoord0.xy - center;
    float c = cos(angle), s = sin(angle);
    uv = vec2(uv.x * c - uv.y * s, uv.x * s + uv.y * c) / zoom + center;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    else if (compat)
        gl_FragColor = vec4(bilinearCompat(s_texColor, uv, textureSize(s_texColor, 0)), 1.0);
    else
        gl_FragColor = texture2D(s_texColor, uv);
}
