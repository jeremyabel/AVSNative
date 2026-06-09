$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x=effect(0-3), y=texelWidth, z=texelHeight, w=unused
uniform vec4 u_mfParams;

float chrome(float c)
{
    return c < 0.5 ? 2.0 * c : 2.0 * (1.0 - c);
}

void main()
{
    vec2 uv    = v_texcoord0.xy;
    vec3 c     = texture2D(s_texColor, uv).rgb;
    int effect = int(u_mfParams.x);

    vec3 result;

    if (effect == 0)
    {
        result = vec3(chrome(c.r), chrome(c.g), chrome(c.b));
    }
    else if (effect == 1)
    {
        vec3 t = vec3(chrome(c.r), chrome(c.g), chrome(c.b));
        result = vec3(chrome(t.r), chrome(t.g), chrome(t.b));
    }
    else if (effect == 2)
    {
        vec3 t  = vec3(chrome(c.r), chrome(c.g), chrome(c.b));
        vec3 t2 = vec3(chrome(t.r), chrome(t.g), chrome(t.b));
        result  = vec3(chrome(t2.r), chrome(t2.g), chrome(t2.b));
    }
    else
    {
        // Infroot + Border Convolution:
        // pixel is white if self OR right OR below neighbor is non-zero
        // bgfx/Vulkan UV: (0,0)=top-left, so "below" = +texelHeight
        vec3 self  = c;
        vec3 right = texture2D(s_texColor, uv + vec2( u_mfParams.y, 0.0)).rgb;
        vec3 below = texture2D(s_texColor, uv + vec2(0.0,  u_mfParams.z)).rgb;
        bool hit = (self.r  > 0.0 || self.g  > 0.0 || self.b  > 0.0)
                || (right.r > 0.0 || right.g > 0.0 || right.b > 0.0)
                || (below.r > 0.0 || below.g > 0.0 || below.b > 0.0);
        result = hit ? vec3(1.0, 1.0, 1.0) : vec3(0.0, 0.0, 0.0);
    }

    gl_FragColor = vec4(result, 1.0);
}
