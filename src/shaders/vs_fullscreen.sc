$input a_position
$output v_texcoord0

#include <bgfx_shader.sh>

// Fullscreen triangle: vertices span NDC [-1,1] with one large triangle.
// UV is derived from position so no texcoord attribute is needed.
void main()
{
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    
    // UV (0,0) = top-left to match Vulkan texture convention.
    // NDC y=+1 is top-of-screen, so we invert y when deriving UV from position.
    v_texcoord0 = vec4(a_position.x * 0.5 + 0.5, 0.5 - a_position.y * 0.5, 0.0, 0.0);
}
