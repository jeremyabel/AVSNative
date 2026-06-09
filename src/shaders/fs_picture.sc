$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_input, 0);
SAMPLER2D(s_image, 1);
uniform vec4 u_picParams; // x=blend(0=replace,1=add,2=50/50) y=fit(0=stretch,1=fw,2=fh) z=imgAspect w=scrAspect

void main()
{
    vec2 uv = v_texcoord0.xy;

    float blend     = u_picParams.x;
    float fit       = u_picParams.y;
    float imgAspect = u_picParams.z;
    float scrAspect = u_picParams.w;

    vec4 base  = texture2D(s_input, uv);
    vec2 imgUv = uv;
    float inside = 1.0;

    if (fit > 0.5 && fit < 1.5)
    {
        // Fit Width: height scaled proportionally and centered
        float hFrac = scrAspect / imgAspect;
        float y0 = 0.5 - hFrac * 0.5;
        float y1 = 0.5 + hFrac * 0.5;
        if (uv.y < y0 || uv.y > y1)
            inside = 0.0;
        else
            imgUv.y = (uv.y - y0) / hFrac;
    }
    else if (fit > 1.5)
    {
        // Fit Height: width scaled proportionally and centered
        float wFrac = imgAspect / scrAspect;
        float x0 = 0.5 - wFrac * 0.5;
        float x1 = 0.5 + wFrac * 0.5;
        if (uv.x < x0 || uv.x > x1)
            inside = 0.0;
        else
            imgUv.x = (uv.x - x0) / wFrac;
    }

    vec4 img;
    if (inside > 0.5)
        img = texture2D(s_image, imgUv);
    else
        img = vec4(0.0, 0.0, 0.0, 1.0);

    vec4 result;
    if (blend > 0.5 && blend < 1.5)
        result = clamp(img + base, 0.0, 1.0);
    else if (blend > 1.5)
        result = img * 0.5 + base * 0.5;
    else
        result = img;

    gl_FragColor = result;
}
