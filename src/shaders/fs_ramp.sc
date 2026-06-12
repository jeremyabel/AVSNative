$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);
SAMPLER2D(s_lut,   1);   // 256x1 RGBA8, point-sampled

// x=type(0=linear,1=radial,2=diamond,3=square), y=scale, z=rotation(radians), w=blend(0-2)
uniform vec4 u_rampParams;
// x=aspect (width/height)
uniform vec4 u_rampParams2;

void main()
{
    int   type   = int(u_rampParams.x + 0.5);
    float scale  = u_rampParams.y;
    float rot    = u_rampParams.z;
    int   blend  = int(u_rampParams.w + 0.5);
    float aspect = u_rampParams2.x;

    // Centered coords in [-1,1]. Symmetric shapes get aspect correction so they
    // aren't stretched on non-square output; linear keeps full edge-to-edge span.
    vec2 p = (v_texcoord0.xy - 0.5) * 2.0;
    if (type != 0)
        p.x *= aspect;

    // Rotate the coordinate frame, then scale.
    float c = cos(rot), s = sin(rot);
    vec2  q = vec2(p.x * c - p.y * s, p.x * s + p.y * c) * scale;

    float t;
    if      (type == 1) t = length(q);                 // radial  (circular)
    else if (type == 2) t = abs(q.x) + abs(q.y);       // diamond (L1)
    else if (type == 3) t = max(abs(q.x), abs(q.y));   // square  (Linf)
    else                t = q.x * 0.5 + 0.5;           // linear

    t = clamp(t, 0.0, 1.0);

    vec3 grad = texture2D(s_lut, vec2((t * 255.0 + 0.5) / 256.0, 0.5)).rgb;
    vec3 orig = texture2D(s_input, v_texcoord0.xy).rgb;

    vec3 result;
    if      (blend == 1) result = min(orig + grad, vec3_splat(1.0));   // additive
    else if (blend == 2) result = (orig + grad) * 0.5;                 // 50/50
    else                 result = grad;                                // replace

    gl_FragColor = vec4(result, 1.0);
}
