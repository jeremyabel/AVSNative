$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// xyz = clip color (0..1)
uniform vec4 u_clipColor;

void main()
{
    vec3 c = texture2D(s_texColor, v_texcoord0.xy).rgb;
    vec3 clip = u_clipColor.xyz;
    
    // 1.0 when every channel of c is <= the clip color, 0.0 otherwise
    float below = step(c.r, clip.r) * step(c.g, clip.g) * step(c.b, clip.b);
    gl_FragColor = vec4(mix(c, clip, below), 1.0);
}
