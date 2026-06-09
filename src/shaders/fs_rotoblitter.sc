$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x=cosTheta, y=sinTheta, z=zoom, w=blend(0/1)
uniform vec4 u_rbTransform;
// x=width, y=height
uniform vec4 u_rbResolution;

void main()
{
    float cosT  = u_rbTransform.x;
    float sinT  = u_rbTransform.y;
    float zoom  = u_rbTransform.z;
    float blend = u_rbTransform.w;
    float W     = u_rbResolution.x;
    float H     = u_rbResolution.y;

    float cx = W * 0.5;
    float cy = H * 0.5;

    // bgfx/Vulkan: gl_FragCoord.y=0 at top, increases downward — y-down already,
    // so no sign flip needed (unlike the WebGL version).
    float dx = gl_FragCoord.x - cx;
    float dy = gl_FragCoord.y - cy;

    float src_px = cx + zoom * (cosT * dx - sinT * dy);
    float src_py = cy + zoom * (sinT * dx + cosT * dy);

    // Tile via fract — matches original s %= (w-1) wrap behaviour
    vec2 src_uv = fract(vec2(src_px / W, src_py / H));

    vec4 mapped = texture2D(s_texColor, src_uv);

    if (blend > 0.5)
    {
        vec4 orig = texture2D(s_texColor, v_texcoord0.xy);
        gl_FragColor = vec4((orig.rgb + mapped.rgb) * 0.5, 1.0);
    }
    else
    {
        gl_FragColor = vec4(mapped.rgb, 1.0);
    }
}
