$input a_position
/*
 * Shadow Pass Vertex Shader
 * Renders geometry from light's perspective
 */

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
