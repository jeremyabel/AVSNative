$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input,   0);  // background (current FBO input)
SAMPLER2D(s_overlay, 1);  // CPU-stamped particle layer (a=255 where painted, 0 elsewhere)

// x=blendMode (0-9), y=alpha (for mode 7)
uniform vec4 u_t2params;

void main()
{
    vec2 uv = v_texcoord0.xy;
    vec3 bg = texture2D(s_input,   uv).rgb;
    vec4 ov = texture2D(s_overlay, uv);

    // Unpainted pixel: pass background through unchanged.
    if (ov.a < 0.5) { gl_FragColor = vec4(bg, 1.0); return; }

    vec3  src   = ov.rgb;
    int   mode  = int(u_t2params.x + 0.5);
    float alpha = u_t2params.y;

    vec3 r;
    if      (mode == 1) r = min(bg + src, vec3(1.0));
    else if (mode == 2) r = max(bg, src);
    else if (mode == 3) r = (bg + src) * 0.5;
    else if (mode == 4) r = max(bg - src, vec3(0.0));
    else if (mode == 5) r = max(src - bg, vec3(0.0));
    else if (mode == 6) r = bg * src;
    else if (mode == 7) r = mix(bg, src, alpha);
    else if (mode == 8) {
        ivec3 a = ivec3(bg  * 255.0 + 0.5);
        ivec3 b = ivec3(src * 255.0 + 0.5);
        r = vec3(a ^ b) / 255.0;
    }
    else if (mode == 9) r = min(bg, src);
    else                r = src;  // 0: Replace

    gl_FragColor = vec4(r, 1.0);
}
