$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_base, 0);
SAMPLER2D(s_src,  1);
SAMPLER2D(s_mask, 2);  // only used for mode 12 (Buffer); dummy-bound otherwise

// x = blend mode (0-13), y = adjustable amount, z = maskInvert (0 or 1)
uniform vec4 u_elBlendParams;

void main()
{
    vec2 uv = v_texcoord0.xy;
    vec3 b = texture2D(s_base, uv).rgb;
    vec3 s = texture2D(s_src,  uv).rgb;
    int  mode       = int(u_elBlendParams.x + 0.5);
    float amt       = u_elBlendParams.y;
    bool maskInvert = u_elBlendParams.z > 0.5;
    vec3 r;
    if      (mode == 0)  r = b;
    else if (mode == 1)  r = s;
    else if (mode == 2)  r = (b + s) * 0.5;
    else if (mode == 3)  r = max(b, s);
    else if (mode == 4)  r = clamp(b + s, 0.0, 1.0);
    else if (mode == 5)  r = clamp(b - s, 0.0, 1.0);
    else if (mode == 6)  r = clamp(s - b, 0.0, 1.0);
    else if (mode == 7)  r = (int(gl_FragCoord.y) % 2 == 0) ? s : b;
    else if (mode == 8)  { ivec2 c = ivec2(gl_FragCoord.xy); r = ((c.x + c.y) % 2 == 0) ? s : b; }
    else if (mode == 9)  { ivec3 bi = ivec3(b * 255.0 + 0.5); ivec3 si = ivec3(s * 255.0 + 0.5); r = vec3(bi ^ si) / 255.0; }
    else if (mode == 10) r = mix(b, s, clamp(amt, 0.0, 1.0));
    else if (mode == 11) r = b * s;
    else if (mode == 12) {
        vec3 mask = texture2D(s_mask, uv).rgb;
        float alpha = max(max(mask.r, mask.g), mask.b);
        if (maskInvert) alpha = 1.0 - alpha;
        r = mix(b, s, alpha);
    }
    else                 r = min(b, s);
    gl_FragColor = vec4(r, 1.0);
}
