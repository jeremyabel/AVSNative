$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x=canvasW, y=canvasH, z=seed
uniform vec4 u_scatterParams;

float h1(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}
float h2(vec2 p)
{
    return fract(sin(dot(p, vec2(269.5, 183.3))) * 43758.5453123);
}

// Map index [0,8) -> offset {-3,-2,-1,0,0,1,2,3}
float fudge(float i)
{
    float v = i - 4.0;
    if (v < 0.0) v += 1.0;
    return v;
}

void main()
{
    float cw   = u_scatterParams.x;
    float ch   = u_scatterParams.y;
    float seed = u_scatterParams.z;

    // gl_FragCoord.y=0 is top in Vulkan/bgfx (opposite of WebGL)
    float px = floor(gl_FragCoord.x);
    float py = floor(gl_FragCoord.y);

    // Pass through 4-row border at top and bottom
    if (py < 4.0 || py >= ch - 4.0)
    {
        gl_FragColor = vec4(texture2D(s_texColor, v_texcoord0.xy).rgb, 1.0);
        return;
    }

    vec2 seed_x = vec2(px + seed * 17.31, py + seed *  5.77);
    vec2 seed_y = vec2(px + seed *  3.13, py + seed * 11.97);
    float ri_x  = floor(h1(seed_x) * 8.0);
    float ri_y  = floor(h2(seed_y) * 8.0);

    float dx = fudge(ri_x);
    float dy = fudge(ri_y);

    vec2 src_uv = vec2((px + dx + 0.5) / cw, (py + dy + 0.5) / ch);
    src_uv = clamp(src_uv, vec2(0.0, 0.0), vec2(1.0, 1.0));

    gl_FragColor = vec4(texture2D(s_texColor, src_uv).rgb, 1.0);
}
