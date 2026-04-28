$input a_position, a_normal, a_texcoord0
$output PixelPosition, PixelNormal, ShadowCoord, TexCoord, PixelTangent, PixelBitangent

#include <bgfx_shader.sh>

uniform mat4 u_shadowMtx;
uniform vec4 u_stylize; // x: cel bands, y: rim scale, z: wobble amp (world), w: wobble phase

void main()
{
    // Canonical order: clip = u_viewProj * (u_model * pos)
    vec4 localPos = vec4(a_position, 1.0);
    vec3 worldPos3 = mul(u_model[0], localPos).xyz;
    if (abs(u_stylize.z) > 0.00001) {
        vec3 wn = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
        float ph = u_stylize.w + dot(worldPos3, vec3(0.73, 0.19, 0.64));
        float disp = u_stylize.z * (0.5 + 0.5 * sin(ph));
        worldPos3 += wn * disp;
    }
    vec4 WorldPos = vec4(worldPos3, 1.0);
    gl_Position = mul(u_viewProj, WorldPos);

    // Remap depth from [-1,1] to [0,1] for D3D-style clip space (Solstice specific fixup)
    // Note: This relies on using standard GL projection matrix passed to u_viewProj
    // Ideally this should be handled in C++ matrix construction or BGFX caps
    #if BGFX_SHADER_LANGUAGE_HLSL
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
    #endif
    ShadowCoord = mul(u_shadowMtx, WorldPos);

    PixelPosition = WorldPos.xyz;

    // Direct float normal (no decoding needed)
    vec3 Nrm = a_normal;
    // Transform normal by model matrix (assuming uniform scale, otherwise use inverse transpose)
    PixelNormal = normalize(mul(u_model[0], vec4(Nrm, 0.0)).xyz);

    // Calculate tangent and bitangent for normal mapping
    // Since tangents are not in vertex data, compute them from normal
    // Use Gram-Schmidt to build orthonormal basis
    vec3 Ref = abs(PixelNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 Tangent = normalize(Ref - PixelNormal * dot(Ref, PixelNormal));
    vec3 Bitangent = cross(PixelNormal, Tangent);

    PixelTangent = Tangent;
    PixelBitangent = Bitangent;

    // Pass through UV coordinates
    TexCoord = a_texcoord0;
}

