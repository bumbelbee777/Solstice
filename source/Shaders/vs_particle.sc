$input a_position, a_color0, a_texcoord0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

void main()
{
    // Particle position in world space
    vec4 worldPos = vec4(a_position, 1.0);

    // Transform to clip space
    gl_Position = mul(u_viewProj, worldPos);

    // Remap depth from [-1,1] to [0,1] for D3D-style clip space
    #if BGFX_SHADER_LANGUAGE_HLSL
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
    #endif

    // Pass through color and UV
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
}
