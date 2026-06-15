$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x=fs1, y=fs2, z=fs3  (integer fader values, -32..32)
uniform vec4 u_cfParams;

void main()
{
    vec3 c = texture2D(s_texColor, v_texcoord0.xy).rgb;
    
    // Round to integer 0..255 before classifying. The win32 original works in
    // integer pixel space, so a gray pixel has R==G==B exactly. Bilinear sampling
    // here introduces sub-LSB per-channel float noise; without rounding, that noise
    // trips the strict ">" channel-dominance test and a neutral gray gets pushed
    // into a colour branch (e.g. cyan). Rounding makes the comparison integer-exact.
    float R = floor(c.r * 255.0 + 0.5);
    float G = floor(c.g * 255.0 + 0.5);
    float B = floor(c.b * 255.0 + 0.5);

    float fs1 = u_cfParams.x;
    float fs2 = u_cfParams.y;
    float fs3 = u_cfParams.z;

    float gMinusB = G - B;
    float bMinusR = B - R;

    float dR, dG, dB;
    if (gMinusB > 0.0 && gMinusB > -bMinusR)
    {
        // green dominant
        dR = fs3;
        dG = fs2;
        dB = fs1;  
    }
    else if (bMinusR < 0.0 && gMinusB < -bMinusR)
    {
        // red dominant
        dR = fs2;
        dG = fs1;
        dB = fs3;
    }
    else if (gMinusB < 0.0 && bMinusR > 0.0)
    {
        // blue dominant
        dR = fs1;
        dG = fs3;
        dB = fs2;
    }
    else
    {
        // neutral
        dR = fs3;
        dG = fs3;
        dB = fs3;
    }

    gl_FragColor = vec4(clamp(c + vec3(dR, dG, dB) / 255.0, vec3_splat(0.0), vec3_splat(1.0)), 1.0);
}
