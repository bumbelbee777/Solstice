$input v_texcoord0

/*
 * Post Processing Fragment Shader
 * ACES Tone Mapping, Vignette, Grading
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor,  0);

// Narkowicz ACES (cheaper)
vec3 aces(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Random noise for dithering
float random(vec2 uv) {
    return fract(sin(dot(uv.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    // 2. Chromatic Aberration (Simulate lens imperfection)
    vec2 center = v_texcoord0 * 2.0 - 1.0;
    float dist = length(center);
    
    // Offset increases with distance from center
    // Reduced significantly (was 0.006) to prevent massive fringing
    vec2 aberOffset = center * 0.0015; 
    
    float r = texture2D(s_texColor, v_texcoord0 - aberOffset).r;
    float g = texture2D(s_texColor, v_texcoord0).g; // Green is anchor
    float b = texture2D(s_texColor, v_texcoord0 + aberOffset).b;
    vec3 color = vec3(r, g, b);

    // 2. Exposure
    color *= 1.3; // Slight boost

    // 3. Simple Luma Sharpening (Cheap)
    // Sample cross pattern
    float sampleDist = 1.0 / 1280.0; // Assume roughly 720p/1080p scale
    vec3 up = texture2D(s_texColor, v_texcoord0 + vec2(0.0, sampleDist)).rgb;
    vec3 down = texture2D(s_texColor, v_texcoord0 + vec2(0.0, -sampleDist)).rgb;
    vec3 left = texture2D(s_texColor, v_texcoord0 + vec2(-sampleDist, 0.0)).rgb;
    vec3 right = texture2D(s_texColor, v_texcoord0 + vec2(sampleDist, 0.0)).rgb;
    
    vec3 blur = (up + down + left + right) * 0.25;
    vec3 sharp = color - blur;
    color = color + sharp * 0.15; // Reduced sharpening (was 0.3)

    // 4. Vignette (Reduced intensity)
    // Soft falloff from 0.4 to 2.5
    float vignette = smoothstep(2.5, 0.4, dist); 
    color *= vignette;
    
    // 5. Grading
    // Contrast
    color = pow(color, vec3_splat(1.1));
    // Saturation
    vec3 gray = vec3_splat(dot(color, vec3(0.299, 0.587, 0.114)));
    color = mix(gray, color, 1.25);

    // 6. Tone Mapping
    vec3 mapped = aces(color);
    
    // 7. Gamma
    mapped = pow(mapped, vec3_splat(1.0/2.2));
    
    // 8. Dither
    float noise = random(v_texcoord0) * 0.005;
    mapped += noise;
    
    gl_FragColor = vec4(mapped, 1.0);
}
