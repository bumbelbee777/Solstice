$input a_position, a_texcoord0
$output TexCoord

/*
 * Raytracing Vertex Shader
 * Fullscreen quad pass-through for raytracing post-processing
 */

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_viewProj, vec4(a_position, 1.0));
	TexCoord = a_texcoord0;
}
