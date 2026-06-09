$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);
SAMPLER2D(s_image, 1);
// x=blend(0-10), y=adjust(0..1)
uniform vec4 u_p2Params;

void main()
{
    vec2 uv   = v_texcoord0.xy;
    int  mode = int(u_p2Params.x + 0.5);
    float adj = u_p2Params.y;

    vec3 base = texture2D(s_input, uv).rgb;
    vec3 img  = texture2D(s_image, uv).rgb;
    vec3 r;

    if      (mode == 0) r = img;
    else if (mode == 1) r = clamp(img + base, 0.0, 1.0);
    else if (mode == 2) r = max(img, base);
    else if (mode == 3) r = img * 0.5 + base * 0.5;
    else if (mode == 4) r = max(base - img, vec3(0.0, 0.0, 0.0));
    else if (mode == 5) r = max(img - base, vec3(0.0, 0.0, 0.0));
    else if (mode == 6) r = img * base;
    else if (mode == 7) r = mix(base, img, adj);
    else if (mode == 8)
    {
        ivec3 ia = ivec3(img  * 255.0 + 0.5);
        ivec3 ba = ivec3(base * 255.0 + 0.5);
        r = vec3(ia ^ ba) / 255.0;
    }
    else if (mode == 9) r = min(img, base);
    else                r = base;   // 10: Ignore — handled by early return in C++

    gl_FragColor = vec4(r, 1.0);
}
