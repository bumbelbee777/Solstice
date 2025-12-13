/*
 * Shadow Pass Fragment Shader
 * Empty - we only care about depth writing
 */

#include <bgfx_shader.sh>

void main()
{
    // Depth is implicitly written
    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
