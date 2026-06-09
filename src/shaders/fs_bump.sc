$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);

// (1/w, 1/h, w, h)
uniform vec4 u_texelSize;

// (center_x_px, center_y_px, depthScaled, blendMode)
// blendMode: 0=Replace  1=Additive  2=50/50
uniform vec4 u_bumpParams;

// (invert, showLightPos, 0, 0)
uniform vec4 u_bumpFlags;

float maxCh(vec3 c) { return max(c.r, max(c.g, c.b)); }

void main()
{
    vec2 uv      = v_texcoord0.xy;
    vec2 texelSz = u_texelSize.xy;

    // Border pixels → black (original memsets output to 0 and skips the 1-px border).
    if (uv.x < texelSz.x || uv.x > 1.0 - texelSz.x ||
        uv.y < texelSz.y || uv.y > 1.0 - texelSz.y)
    {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Sample brightness of the four cardinal neighbors (point-sampled by the caller).
    float mL = maxCh(texture2D(s_input, uv - vec2(texelSz.x, 0.0      )).rgb) * 255.0;
    float mR = maxCh(texture2D(s_input, uv + vec2(texelSz.x, 0.0      )).rgb) * 255.0;
    float mA = maxCh(texture2D(s_input, uv - vec2(0.0,       texelSz.y)).rgb) * 255.0; // above (row-1)
    float mB = maxCh(texture2D(s_input, uv + vec2(0.0,       texelSz.y)).rgb) * 255.0; // below (row+1)

    if (u_bumpFlags.x > 0.5)
    {
        mL = 255.0 - mL;
        mR = 255.0 - mR;
        mA = 255.0 - mA;
        mB = 255.0 - mB;
    }

    // Light offset in pixel units.
    // bgfx y=0 = top-left matches original C row-0-at-top — no Y-flip needed.
    float px = uv.x * u_texelSize.z - 0.5;
    float py = uv.y * u_texelSize.w - 0.5;
    float lX = px - u_bumpParams.x;
    float lY = py - u_bumpParams.y;

    float xDist = 127.0 - abs((mR - mL) - lX);
    float yDist = 127.0 - abs((mB - mA) - lY);

    vec3 selfInt = texture2D(s_input, uv).rgb * 255.0;

    vec3 computed;
    if (xDist <= 0.0 || yDist <= 0.0)
        computed = min(selfInt, vec3(254.0, 254.0, 254.0));
    else
        computed = min(selfInt + (xDist * yDist * u_bumpParams.z) / 16384.0,
                       vec3(254.0, 254.0, 254.0));

    float bm = u_bumpParams.w;
    vec3 result;
    if (bm > 0.5 && bm < 1.5)
        result = min(computed + selfInt, vec3(255.0, 255.0, 255.0)) / 255.0;  // Additive
    else if (bm > 1.5)
        result = (computed + selfInt) / (2.0 * 255.0);                        // 50/50
    else
        result = computed / 255.0;                                             // Replace

    // Show light position: white dot at the exact center pixel.
    if (u_bumpFlags.y > 0.5 && abs(lX) < 0.5 && abs(lY) < 0.5)
        result = vec3(1.0, 1.0, 1.0);

    gl_FragColor = vec4(result, 1.0);
}
