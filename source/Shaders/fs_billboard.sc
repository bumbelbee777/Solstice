$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

uniform vec4 u_ColorTint; // Color tint (rgba) - defaults to white

void main()
{
    vec4 texColor = texture2D(s_texColor, v_texcoord0);
    gl_FragColor = texColor * u_ColorTint;
}
