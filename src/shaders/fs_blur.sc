$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// xy = texel step in blur direction (1/w,0) or (0,1/h), z = radius
uniform vec4 u_blurParams;

void main()
{
    vec2 dir = u_blurParams.xy;
    int radius = int(u_blurParams.z + 0.5);
    vec3 sum = vec3_splat(0.0);
    float total = 0.0;

    for (int i = -radius; i <= radius; i++)
    {
        float w = float(radius + 1 - abs(i));
        sum += texture2D(s_texColor, v_texcoord0.xy + dir * float(i)).rgb * w;
        total += w;
    }

    gl_FragColor = vec4(sum / total, 1.0);
}
