$input a_position
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_xform;   // xy = center (pixels), zw = half-size (pixels)
uniform vec4 u_rot;     // x = cos, y = sin, z = flipX, w = flipY
uniform vec4 u_screen;  // xy = (width, height) in pixels

// Builds the particle quad from a unit quad ([-1,1] corners). Scale + rotate happen
// in pixel space (so a square sprite stays square on a non-square viewport), then the
// pixel position is converted to NDC. UV is derived from the corner with optional flip.
void main()
{
    vec2 corner = a_position.xy;            // [-1, 1]
    vec2 off    = corner * u_xform.zw;      // half-extent offset (pixels)
    vec2 rp     = vec2(off.x * u_rot.x - off.y * u_rot.y,
                       off.x * u_rot.y + off.y * u_rot.x);
    vec2 px     = u_xform.xy + rp;
    vec2 ndc    = vec2(px.x / u_screen.x * 2.0 - 1.0,
                       1.0 - px.y / u_screen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);

    vec2 uv = corner * 0.5 + 0.5;
    if (u_rot.z > 0.5) uv.x = 1.0 - uv.x;
    if (u_rot.w > 0.5) uv.y = 1.0 - uv.y;
    v_texcoord0 = vec4(uv, 0.0, 0.0);
}
