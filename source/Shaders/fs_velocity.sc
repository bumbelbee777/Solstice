$input TexCoord

#include <bgfx_shader.sh>

void main()
{
    gl_FragColor = vec4(TexCoord, 0.0, 1.0);
}
