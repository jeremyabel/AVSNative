$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input,  0);
SAMPLER2D(s_minmax, 1);   // 1x1 global (min,max)

// Apply pass: remap input using the 1x1 global (min,max) result.
// If max == min the image is flat -> output black (matches original AVS behaviour).
void main()
{
    vec2  uv  = v_texcoord0.xy;
    vec4  mm  = texture2D(s_minmax, vec2(0.5, 0.5));
    float mn  = mm.r;
    float mx  = mm.g;
    vec4  px  = texture2D(s_input, uv);
    float rng = mx - mn;
    if (rng > 0.0)
        px.rgb = clamp((px.rgb - mn) / rng, 0.0, 1.0);
    else
        px.rgb = vec3(0.0, 0.0, 0.0);
    gl_FragColor = px;
}
