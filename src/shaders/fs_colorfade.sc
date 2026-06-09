$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

// x=fs1, y=fs2, z=fs3  (integer fader values, -32..32)
uniform vec4 u_cfParams;

void main()
{
    vec3  c  = texture2D(s_texColor, v_texcoord0.xy).rgb;
    float R  = c.r * 255.0;
    float G  = c.g * 255.0;
    float B  = c.b * 255.0;

    float fs1 = u_cfParams.x;
    float fs2 = u_cfParams.y;
    float fs3 = u_cfParams.z;

    float gMinusB = G - B;
    float bMinusR = B - R;

    float dR, dG, dB;
    if (gMinusB > 0.0 && gMinusB > -bMinusR)
    {
        dR = fs3; dG = fs2; dB = fs1;   // green dominant
    }
    else if (bMinusR < 0.0 && gMinusB < -bMinusR)
    {
        dR = fs2; dG = fs1; dB = fs3;   // red dominant
    }
    else if (gMinusB < 0.0 && bMinusR > 0.0)
    {
        dR = fs1; dG = fs3; dB = fs2;   // blue dominant
    }
    else
    {
        dR = fs3; dG = fs3; dB = fs3;   // neutral
    }

    gl_FragColor = vec4(clamp(c + vec3(dR, dG, dB) / 255.0, vec3_splat(0.0), vec3_splat(1.0)), 1.0);
}
