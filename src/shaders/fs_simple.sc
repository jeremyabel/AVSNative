$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input,   0);
SAMPLER2D(s_overlay, 1);

// x = blend mode (0-9, from SetRenderMode)
// y = alpha (0-255, used by Adjustable mode 7)
uniform vec4 u_simpleParams;

void main()
{
    vec2 uv   = v_texcoord0.xy;
    vec4 base = texture2D(s_input,   uv);
    vec4 over = texture2D(s_overlay, uv);

    // NanoVG renders with premultiplied alpha.
    // Un-premultiply to recover the actual drawn line color for blend operations.
    vec3 lineCol = over.rgb / max(over.a, 0.00001);

    int   mode = int(u_simpleParams.x + 0.5);
    float amt  = u_simpleParams.y / 255.0; // Adjustable blend amount

    vec3 blended;
    if      (mode == 1) blended = clamp(base.rgb + lineCol, vec3_splat(0.0), vec3_splat(1.0)); // Add
    else if (mode == 2) blended = max(base.rgb, lineCol);                                       // Max
    else if (mode == 3) blended = (base.rgb + lineCol) * 0.5;                                   // 50/50
    else if (mode == 4) blended = clamp(base.rgb - lineCol, vec3_splat(0.0), vec3_splat(1.0)); // Sub1 (base-line)
    else if (mode == 5) blended = clamp(lineCol - base.rgb, vec3_splat(0.0), vec3_splat(1.0)); // Sub2 (line-base)
    else if (mode == 6) blended = base.rgb * lineCol;                                           // Multiply
    else if (mode == 7) blended = mix(base.rgb, lineCol, amt);                                  // Adjustable
    else if (mode == 8)                                                                          // XOR (8-bit)
    {
        ivec3 ib = ivec3(base.rgb * 255.0 + 0.5);
        ivec3 il = ivec3(lineCol  * 255.0 + 0.5);
        blended = vec3(ib ^ il) / 255.0;
    }
    else if (mode == 9) blended = min(base.rgb, lineCol);                                       // Minimum
    else                blended = lineCol;                                                       // Replace (0)

    // over.a weights the blend: 0 = no overlay (keep base), 1 = fully blended.
    // This preserves NanoVG anti-aliasing at stroke edges.
    gl_FragColor = vec4(mix(base.rgb, blended, over.a), 1.0);
}
