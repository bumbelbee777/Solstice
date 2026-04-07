$input a_position, a_color0
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_viewProj, vec4(a_position, 1.0));
    // Match vs_standard.sc: D3D clip-space Z remap for consistent depth with the scene pass.
#if BGFX_SHADER_LANGUAGE_HLSL
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
#endif
    v_color0 = a_color0;
}

