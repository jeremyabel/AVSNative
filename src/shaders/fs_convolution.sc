$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);

// 49 kernel weights packed into 13 vec4s (52 floats; last 3 unused).
uniform vec4 u_kernel[13];
// x=bias(=256*Bias), y=scale(signed), z=wrap(0/1), w=absolute(0/1)
uniform vec4 u_convParams;
// x=twoPass(0/1), y=texelX, z=texelY, w=unused
uniform vec4 u_convParams2;

// This mirrors the win32 e_convolution MMX pipeline, which works on 16-bit
// integers (pixels 0..255). Matching it bit-for-bit matters because wrap /
// absolute / negative-scale all rely on integer + unsigned-shift quirks:
//   * default  (psubusw): negative result saturates to 0 (black)
//   * wrap     (psubw):   negative wraps through the unsigned scale → white
//   * absolute (psubsw + &0x7FFF): negative "throws away the sign" → white
//   * scale<0: kernel sign-swapped, divide by |scale| (so it inverts)
// Final packuswb reinterprets the 16-bit word as SIGNED, saturating to [0,255].

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

// 7x7 weighted sum in integer pixel units (Σ kernel·pixel, pixels 0..255).
// rotated => 90° CCW: kernel[row][col] reads from (3-row, col-3).
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
            // Round to an exact 0..255 pixel (the source is RGBA8 at a texel
            // centre, so this is exact) to match the integer-domain original.
            vec3 px = floor(texture2D(s_input, uv + off * texel).rgb * 255.0 + 0.5);
            acc += w * px;
        }
    }
    return acc;
}

// Per-pass combine of the signed accumulator into an unsigned 16-bit word.
vec3 combine(vec3 v, int wrap, int absolute)
{
    if (absolute == 1)
    {
        // psubsw (saturating signed) then clear the sign bit (& 0x7FFF):
        // d>=0 -> d ; d<0 -> d+32768  (this is the original "throw away sign").
        vec3 d = clamp(v, vec3_splat(-32768.0), vec3_splat(32767.0));
        return mix(d + 32768.0, d, step(vec3_splat(0.0), d));
    }
    if (wrap == 1)
        return v - 65536.0 * floor(v / 65536.0);            // psubw: mod 65536
    return clamp(v, vec3_splat(0.0), vec3_splat(65535.0));   // psubusw: saturate to 0
}

// packuswb: interpret the 16-bit word as SIGNED, saturate to [0,255].
vec3 packuswb(vec3 u)
{
    vec3 isNeg = step(32768.0, u);                  // u>=32768 => negative
    return mix(min(u, vec3_splat(255.0)), vec3_splat(0.0), isNeg);
}

void main()
{
    float bias     = u_convParams.x;                // already 256*Bias
    float scale    = u_convParams.y;                // signed
    int   wrap     = int(u_convParams.z + 0.5);
    int   absolute = int(u_convParams.w + 0.5);
    int   twoPass  = int(u_convParams2.x + 0.5);
    vec2  texel    = u_convParams2.yz;
    vec2  uv       = v_texcoord0.xy;

    float scaleAbs = abs(scale);

    // v = Σ kernel·pixel + 256*bias, in integer pixel units. Negative scale
    // sign-swaps the whole accumulator (kernel and bias) before the combine.
    vec3 v0 = doPass(false, texel, uv) + bias;
    if (scale < 0.0) v0 = -v0;

    vec3 outv;
    if (twoPass == 1)
    {
        vec3 v1 = doPass(true, texel, uv) + bias;
        if (scale < 0.0) v1 = -v1;
        // Each pass combines first, then psubusw(pass1 - pass0).
        outv = max(combine(v1, wrap, absolute) - combine(v0, wrap, absolute),
                   vec3_splat(0.0));
    }
    else
    {
        outv = combine(v0, wrap, absolute);
    }

    // Unsigned scale division (psrlw / reciprocal mul in the original).
    if (scaleAbs != 1.0)
        outv = floor(outv / scaleAbs);

    gl_FragColor = vec4(packuswb(outv) / 255.0, 1.0);
}
