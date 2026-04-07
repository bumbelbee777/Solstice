$input a_position
$output v_clip

#include <bgfx_shader.sh>

void main()
{
	gl_Position = vec4(a_position.xy, 0.0, 1.0);
	v_clip = a_position.xy;
}
