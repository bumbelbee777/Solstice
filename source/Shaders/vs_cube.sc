$input a_position, a_normal
$output v_position, v_normal, v_shadowcoord

#include <bgfx_shader.sh>

uniform mat4 u_shadowMtx;



void main()
{
    // Canonical order: clip = u_viewProj * (u_model * pos)
    // Shadow coordinates
    // We expect u_shadowMtx to be passed by the application
    // mat4 u_shadowMtx; // Defined in uniform buffer
    
    // Calculate shadow position
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    gl_Position = mul(u_viewProj, worldPos);
    
    // Remap depth from [-1,1] to [0,1] for D3D-style clip space (Solstice specific fixup)
    // Note: This relies on using standard GL projection matrix passed to u_viewProj
    // Ideally this should be handled in C++ matrix construction or BGFX caps
    #if BGFX_SHADER_LANGUAGE_HLSL
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
    #endif
    v_shadowcoord = mul(u_shadowMtx, worldPos);
    
    v_position = worldPos.xyz;

    // Direct float normal (no decoding needed)
    vec3 nrm = a_normal;
    // Transform normal by model matrix (assuming uniform scale, otherwise use inverse transpose)
    v_normal = normalize(mul(u_model[0], vec4(nrm, 0.0)).xyz);
}
