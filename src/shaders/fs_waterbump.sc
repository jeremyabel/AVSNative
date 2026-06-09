$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input,  0);  // color input (point-sampled at render time)
SAMPLER2D(s_height, 1);  // R32F CPU height field; row 0 = top (matches bgfx UV top-left origin)

// (1/w, 1/h, 0, 0) — one texel in UV space
uniform vec4 u_texelSize;

void main()
{
    vec2 uv       = v_texcoord0.xy;
    vec2 texelSz  = u_texelSize.xy;

    // Pass border pixels through unchanged, matching the original's skipped border loop.
    if (uv.x < texelSz.x || uv.x > 1.0 - texelSz.x ||
        uv.y < texelSz.y || uv.y > 1.0 - texelSz.y)
    {
        gl_FragColor = texture2D(s_input, uv);
        return;
    }

    // Sample height at current pixel and its right/down neighbors.
    // Height texture uses NEAREST sampling so int-as-float values are exact.
    float h0 = texture2D(s_height, uv).r;
    float hR = texture2D(s_height, uv + vec2(texelSz.x, 0.0      )).r;
    float hD = texture2D(s_height, uv + vec2(0.0,       texelSz.y)).r;

    // Arithmetic right-shift by 3 (floor(x/8)).  h values are integer-valued floats.
    float dx = floor((h0 - hR) * 0.125);
    float dy = floor((h0 - hD) * 0.125);

    vec2 srcUV = clamp(uv + vec2(dx, dy) * texelSz, vec2(0.0, 0.0), vec2(1.0, 1.0));
    gl_FragColor = texture2D(s_input, srcUV);
}
