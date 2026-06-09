$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_base,    0);
SAMPLER2D(s_overlay, 1);

// x = blend mode (0-9, from SetRenderMode / LineBlendMode bits 0-7)
// y = alpha (0-255, used by Adjustable mode 7)
uniform vec4 u_ssParams;

void main()
{
    vec4 base = texture2D(s_base,    v_texcoord0.xy);
    vec4 over = texture2D(s_overlay, v_texcoord0.xy);

    if (over.a < 0.5)
    {
        gl_FragColor = base;
        return;
    }

    vec3  lineCol = over.rgb;
    int   mode    = int(u_ssParams.x + 0.5);
    float amt     = u_ssParams.y / 255.0;

    vec3 blended;
    if      (mode == 1) blended = clamp(base.rgb + lineCol, vec3_splat(0.0), vec3_splat(1.0));
    else if (mode == 2) blended = max(base.rgb, lineCol);
    else if (mode == 3) blended = (base.rgb + lineCol) * 0.5;
    else if (mode == 4) blended = clamp(base.rgb - lineCol, vec3_splat(0.0), vec3_splat(1.0));
    else if (mode == 5) blended = clamp(lineCol - base.rgb, vec3_splat(0.0), vec3_splat(1.0));
    else if (mode == 6) blended = base.rgb * lineCol;
    else if (mode == 7) blended = mix(base.rgb, lineCol, amt);
    else if (mode == 8)
    {
        ivec3 ib = ivec3(base.rgb * 255.0 + 0.5);
        ivec3 il = ivec3(lineCol  * 255.0 + 0.5);
        blended = vec3(ib ^ il) / 255.0;
    }
    else if (mode == 9) blended = min(base.rgb, lineCol);
    else                blended = lineCol;

    gl_FragColor = vec4(blended, 1.0);
}
