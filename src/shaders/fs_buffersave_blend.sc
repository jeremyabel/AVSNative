$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_base, 0);
SAMPLER2D(s_src,  1);

// x = blend mode (0–11), y = adjustable blend amount
uniform vec4 u_blendParams;

void main()
{
    vec2 uv = v_texcoord0.xy;
    vec3 b = texture2D(s_base, uv).rgb;
    vec3 s = texture2D(s_src,  uv).rgb;
    int  mode = int(u_blendParams.x + 0.5);
    float amt = u_blendParams.y;
    vec3 r;
    if      (mode == 0)  r = s;
    else if (mode == 1)  r = clamp(b + s, 0.0, 1.0);
    else if (mode == 2)  r = max(b, s);
    else if (mode == 3)  r = (b + s) * 0.5;
    else if (mode == 4)  r = b * s;
    else if (mode == 5)  r = clamp(b - s, 0.0, 1.0);
    else if (mode == 6)  r = mix(b, s, amt);
    else if (mode == 7)  r = min(b, s);
    else if (mode == 8)  { ivec2 c = ivec2(gl_FragCoord.xy); r = ((c.x + c.y) % 2 == 0) ? s : b; }
    else if (mode == 9)  r = (int(gl_FragCoord.y) % 2 == 0) ? s : b;
    else if (mode == 10) r = clamp(s - b, 0.0, 1.0);
    else if (mode == 11) { ivec3 bi = ivec3(round(b * 255.0)); ivec3 si = ivec3(round(s * 255.0)); r = vec3(bi ^ si) / 255.0; }
    else                 r = b;
    gl_FragColor = vec4(r, 1.0);
}
