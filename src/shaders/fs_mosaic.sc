$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x = blockSize, y = resWidth, z = resHeight
uniform vec4 u_mosaicParams;

void main()
{
    vec2 res = u_mosaicParams.yz;
    float blockSize = u_mosaicParams.x;

    vec2 px = floor(v_texcoord0.xy * res / blockSize) * blockSize + blockSize * 0.5;
    gl_FragColor = texture2D(s_texColor, px / res);
}
