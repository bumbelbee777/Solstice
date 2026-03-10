$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

float random(vec2 uv) {
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

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

    // Softer circular falloff (more visible core, smoother edge)
    float edge = smoothstep(0.25, 0.78, dist);
    float alpha = pow(1.0 - edge, 1.35);

    // Subtle noise to break uniformity
    float n = random(uv * 12.7);
    alpha *= mix(0.85, 1.0, n);

    // Apply alpha to color
    color.a *= alpha;

    // Slight core boost for prominence
    float glow = 1.0 - dist * 0.6;
    color.rgb *= (1.0 + glow * 0.3);

    gl_FragColor = color;
}
