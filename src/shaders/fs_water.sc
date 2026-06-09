$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);  // current input
SAMPLER2D(s_prevTex,  1);  // previous frame input

// x=texelW(1/w), y=texelH(1/h)
uniform vec4 u_waterParams;

void main()
{
    vec2  uv     = v_texcoord0.xy;
    float tw     = u_waterParams.x;
    float th     = u_waterParams.y;

    // Border detection: pixel is on an edge if its UV center falls within one texel of the edge
    bool atL = uv.x < tw;
    bool atR = uv.x > 1.0 - tw;
    bool atT = uv.y < th;
    bool atB = uv.y > 1.0 - th;

    // Sum present neighbors (4-connected)
    vec3 sum = vec3(0.0, 0.0, 0.0);
    if (!atL) sum += texture2D(s_texColor, uv - vec2(tw,  0.0)).rgb;
    if (!atR) sum += texture2D(s_texColor, uv + vec2(tw,  0.0)).rgb;
    if (!atT) sum += texture2D(s_texColor, uv - vec2(0.0, th )).rgb;
    if (!atB) sum += texture2D(s_texColor, uv + vec2(0.0, th )).rgb;

    // Corners (2 neighbors) are not halved; edges and interior (3-4 neighbors) are divided by 2
    bool isCorner = (atL || atR) && (atT || atB);
    if (!isCorner) sum *= 0.5;

    vec3 prev = texture2D(s_prevTex, uv).rgb;
    gl_FragColor = vec4(clamp(sum - prev, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0)), 1.0);
}
