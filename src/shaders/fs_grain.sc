$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x=amount (0-100), y=blendMode (0=Replace 1=Additive 2=50/50), z=isStatic, w=frame
uniform vec4 u_grainParams;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
    vec3 src = texture2D(s_texColor, v_texcoord0.xy).rgb;

    // Skip black pixels, matching original's if(*p) guard.
    if (dot(src, src) == 0.0)
    {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float amount    = u_grainParams.x;
    int   blendMode = int(u_grainParams.y);
    bool  isStatic  = u_grainParams.z > 0.5;
    float frame     = u_grainParams.w;

    vec2 seed = isStatic
        ? v_texcoord0.xy
        : v_texcoord0.xy + vec2(fract(frame * 0.1376), fract(frame * 0.2141));

    float prob  = hash(seed);
    float scale = hash(seed + vec2(5.3, 9.1));

    float threshold = amount / 100.0;
    vec3 c = (prob < threshold) ? src * scale : vec3(0.0, 0.0, 0.0);

    vec3 result;
    if (blendMode == 1)
        result = min(src + c, vec3(1.0, 1.0, 1.0));
    else if (blendMode == 2)
        result = (src + c) * 0.5;
    else
        result = c;

    gl_FragColor = vec4(result, 1.0);
}
