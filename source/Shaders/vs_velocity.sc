$input a_position
$output TexCoord

#include <bgfx_shader.sh>

uniform mat4 u_currViewProj;
uniform mat4 u_prevViewProjVelocity;

void main()
{
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    vec4 currClip = mul(u_currViewProj, worldPos);
    vec4 prevClip = mul(u_prevViewProjVelocity, worldPos);

    vec2 currUv = currClip.xy / max(currClip.w, 0.0001) * 0.5 + 0.5;
    vec2 prevUv = prevClip.xy / max(prevClip.w, 0.0001) * 0.5 + 0.5;
    TexCoord = currUv - prevUv;

    gl_Position = currClip;
}
