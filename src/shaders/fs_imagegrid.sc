$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);   // current framebuffer (for blending)
SAMPLER2D(s_image, 1);   // tiled image (repeat wrap)

uniform vec4 u_gridXform;   // x=transX  y=transY  z=sizeX  w=sizeY   (sizes are tile scale/zoom)
uniform vec4 u_gridParams;  // x=rotation(rad)  y=imgAspect(w/h)  z=scrAspect(w/h)  w=blendMode

void main()
{
    vec2  uv        = v_texcoord0.xy;
    vec2  trans     = u_gridXform.xy;
    vec2  tileScale = u_gridXform.zw;
    float rot       = u_gridParams.x;
    float imgAspect = u_gridParams.y;
    float scrAspect = u_gridParams.z;
    float blendMode = u_gridParams.w;

    // Centre and correct for the viewport aspect so rotation is not skewed.
    vec2 p = uv - 0.5;
    p.x *= scrAspect;

    // Rotate around the centre.
    float c = cos(rot), s = sin(rot);
    p = mat2(c, -s, s, c) * p;

    // Give each tile cell the image's aspect ratio (so the image isn't stretched),
    // then apply the per-axis tile scale (larger size = bigger tile = fewer repeats).
    p.x /= imgAspect;
    p /= tileScale;

    // Translate (in tile units: 1.0 == shift by one whole tile) and tile via fract().
    vec2 imgUv = fract(p + trans + 0.5);

    vec4 img  = texture2D(s_image, imgUv);
    vec4 base = texture2D(s_input, uv);

    vec4 result;
    if (blendMode > 0.5 && blendMode < 1.5)
        result = clamp(img + base, 0.0, 1.0);           // additive
    else if (blendMode > 1.5 && blendMode < 2.5)
        result = img * 0.5 + base * 0.5;                // 50/50
    else if (blendMode > 2.5)
        result = vec4(mix(base.rgb, img.rgb, img.a), 1.0); // alpha
    else
        result = img;                                   // replace

    gl_FragColor = result;
}
