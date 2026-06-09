$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);
SAMPLER2D(s_lut,   1);   // 256x1 RGBA8, point-sampled

// x=colorKey(0-5), y=blendmode(0-9), z=alpha(0..1)
uniform vec4 u_cmParams;

void main()
{
    int   colorKey = int(u_cmParams.x + 0.5);
    int   blend    = int(u_cmParams.y + 0.5);
    float alpha    = u_cmParams.z;

    vec3  orig = texture2D(s_input, v_texcoord0.xy).rgb;
    ivec3 oi   = ivec3(orig * 255.0 + 0.5);

    int key;
    if      (colorKey == 0) key = oi.r;
    else if (colorKey == 1) key = oi.g;
    else if (colorKey == 2) key = oi.b;
    else if (colorKey == 3) key = min((oi.r + oi.g + oi.b) / 2, 255);
    else if (colorKey == 4) key = max(max(oi.r, oi.g), oi.b);
    else                    key = (oi.r + oi.g + oi.b) / 3;

    // Point-sample the 256-wide LUT at the exact key index.
    vec3 mapped = texture2D(s_lut, vec2((float(key) + 0.5) / 256.0, 0.5)).rgb;

    vec3 result;
    if      (blend == 0) result = mapped;
    else if (blend == 1) result = min(orig + mapped, vec3_splat(1.0));
    else if (blend == 2) result = max(orig, mapped);
    else if (blend == 3) result = min(orig, mapped);
    else if (blend == 4) result = (orig + mapped) * 0.5;
    else if (blend == 5) result = max(orig - mapped, vec3_splat(0.0));
    else if (blend == 6) result = max(mapped - orig, vec3_splat(0.0));
    else if (blend == 7) result = orig * mapped;
    else if (blend == 8)
    {
        ivec3 a = ivec3(orig   * 255.0 + 0.5);
        ivec3 b = ivec3(mapped * 255.0 + 0.5);
        result = vec3(a ^ b) / 255.0;
    }
    else result = mix(orig, mapped, alpha);

    gl_FragColor = vec4(result, 1.0);
}
