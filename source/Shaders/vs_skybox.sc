$input a_position
$output PixelPosition

#include <bgfx_shader.sh>

void main()
{
    vec3 worldPos = a_position.xyz;

    // Transform position to clip space
    gl_Position = mul(u_viewProj, mul(u_model[0], vec4(worldPos, 1.0)));

    // Pass world position as direction for cubemap sampling
    // (skybox is centered at origin, so position = direction)
    PixelPosition = worldPos;
}
