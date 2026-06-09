$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// xyz=tint colour (0-1), w=unused
uniform vec4 u_utColor;
// x=invert (0/1), y=blendMode (0=Replace 1=Additive 2=Average)
uniform vec4 u_utParams;

void main()
{
    vec3 orig  = texture2D(s_texColor, v_texcoord0.xy).rgb;

    float depth = max(orig.r, max(orig.g, orig.b));
    if (u_utParams.x > 0.5)
        depth = 1.0 - depth;

    vec3 tinted = u_utColor.rgb * depth;

    int blend = int(u_utParams.y);
    vec3 result;
    if (blend == 1)
        result = clamp(orig + tinted, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
    else if (blend == 2)
        result = (orig + tinted) * 0.5;
    else
        result = tinted;

    gl_FragColor = vec4(result, 1.0);
}
