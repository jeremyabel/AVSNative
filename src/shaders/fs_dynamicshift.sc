$input v_texcoord0

#include <bgfx_shader.sh>
#include "bilinear_compat.sh"

SAMPLER2D(s_input, 0);

uniform vec4 u_ds_params0;  // x=shiftX(px), y=shiftY(px), z=width, w=height
uniform vec4 u_ds_params1;  // x=blend(0/1), y=alpha, z=compat(0/1)

void main()
{
    vec2 uv  = v_texcoord0.xy;
    float sx = u_ds_params0.x;
    float sy = u_ds_params0.y;
    float w  = u_ds_params0.z;
    float h  = u_ds_params0.w;
    float blend = u_ds_params1.x;
    float alpha = u_ds_params1.y;
    bool  compat = u_ds_params1.z > 0.5;

    // Output at (u,v) reads from input shifted by (sx,sy) pixels.
    // Positive sx = content shifts right (border on left).
    // Positive sy = content shifts down  (border on top), Vulkan y-down coords.
    vec2 src_uv = vec2(uv.x - sx / w, uv.y - sy / h);
    bool inBounds = src_uv.x >= 0.0 && src_uv.x <= 1.0
                 && src_uv.y >= 0.0 && src_uv.y <= 1.0;

    vec3 shifted = vec3(0.0, 0.0, 0.0);
    if (inBounds)
        shifted = compat
            ? bilinearCompat(s_input, src_uv, textureSize(s_input, 0))
            : texture2D(s_input, src_uv).rgb;

    if (blend > 0.5) {
        vec3 orig   = texture2D(s_input, uv).rgb;
        gl_FragColor = vec4(shifted * alpha + orig * (1.0 - alpha), 1.0);
    } else {
        gl_FragColor = vec4(shifted, 1.0);
    }
}
