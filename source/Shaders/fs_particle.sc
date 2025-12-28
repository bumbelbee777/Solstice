$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

void main()
{
    // Get color from vertex
    vec4 color = v_color0;

    // Create soft edges using distance from center
    // Since we're rendering quads, we need to calculate distance from quad center
    // For a simple circular falloff, we'll use the UV coordinates
    // UV should be in [0,1] range for the quad
    vec2 center = vec2(0.5, 0.5);
    vec2 uv = v_texcoord0;
    float dist = distance(uv, center);

    // Create smooth circular falloff (soft edges)
    // dist goes from 0 (center) to ~0.707 (corner)
    float alpha = 1.0 - smoothstep(0.3, 0.707, dist);

    // Apply alpha to color
    color.a *= alpha;

    // Optional: Add subtle glow for snowflakes
    // Slight brightness increase at center
    float glow = 1.0 - dist * 0.3;
    color.rgb *= (1.0 + glow * 0.2);

    gl_FragColor = color;
}
