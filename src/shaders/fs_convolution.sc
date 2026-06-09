$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);

// 49 kernel weights packed into 13 vec4s (52 floats; last 3 unused).
uniform vec4 u_kernel[13];
// x=bias, y=scale, z=wrap(0/1), w=absolute(0/1)
uniform vec4 u_convParams;
// x=twoPass(0/1), y=texelX, z=texelY, w=unused
uniform vec4 u_convParams2;

// Fetch kernel weight i (0..48) without dynamic vec4-component indexing.
float kget(int i)
{
    vec4 v = u_kernel[i / 4];
    int  c = i - (i / 4) * 4;
    if (c == 0) return v.x;
    if (c == 1) return v.y;
    if (c == 2) return v.z;
    return v.w;
}

// 7x7 weighted sum. rotated => 90° CCW: kernel[row][col] reads from (3-row, col-3).
vec3 doPass(bool rotated, vec2 texel, vec2 uv)
{
    vec3 acc = vec3_splat(0.0);
    for (int row = 0; row < 7; row++)
    {
        for (int col = 0; col < 7; col++)
        {
            float w = kget(row * 7 + col);
            vec2 off;
            if (rotated) off = vec2(float(3 - row), float(col - 3));
            else         off = vec2(float(col - 3), float(row - 3));
            acc += w * texture2D(s_input, uv + off * texel).rgb;
        }
    }
    return acc;
}

// Matches the original MMX per-pass combine: psubsw+abs / psubw / psubusw.
vec3 perPass(vec3 s, int wrap, int absolute)
{
    if (absolute == 1) return abs(s);
    if (wrap     == 1) return s;                  // signed, no saturation
    return max(s, vec3_splat(0.0));               // saturate to zero
}

void main()
{
    float bias     = u_convParams.x;
    float scale    = u_convParams.y;
    int   wrap     = int(u_convParams.z + 0.5);
    int   absolute = int(u_convParams.w + 0.5);
    int   twoPass  = int(u_convParams2.x + 0.5);
    vec2  texel    = u_convParams2.yz;
    vec2  uv       = v_texcoord0.xy;

    vec3 s0 = doPass(false, texel, uv) + bias;

    vec3 raw;
    if (twoPass == 1)
    {
        vec3 s1 = doPass(true, texel, uv) + bias;
        raw = max(perPass(s1, wrap, absolute) - perPass(s0, wrap, absolute), vec3_splat(0.0)) / scale;
    }
    else
    {
        raw = perPass(s0, wrap, absolute) / scale;
    }

    gl_FragColor = vec4(clamp(raw, 0.0, 1.0), 1.0);
}
