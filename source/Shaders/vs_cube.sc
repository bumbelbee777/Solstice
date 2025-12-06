$input a_position, a_normal
$output v_position, v_normal

#include <bgfx_shader.sh>



void main()
{
    // Canonical order: clip = u_viewProj * (u_model * pos)
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    gl_Position = mul(u_viewProj, worldPos);
    // Remap depth from [-1,1] to [0,1] for D3D-style clip space
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
    
    v_position = worldPos.xyz;

    // Direct float normal (no decoding needed)
    vec3 nrm = a_normal;
    // Transform normal by model matrix (assuming uniform scale, otherwise use inverse transpose)
    v_normal = normalize(mul(u_model[0], vec4(nrm, 0.0)).xyz);
}
