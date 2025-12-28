$input PixelPosition

#include <bgfx_shader.sh>

SAMPLERCUBE(s_texSkybox, 0);

// Cheap hash for noise - works with float or vec3
float hash(float n) {
    n = fract(n * 0.3183099 + 0.1);
    n *= 17.0;
    return fract(n * n * (n + 1.0));
}

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// Simple 3D noise (cheap approximation)
float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float n = i.x + i.y * 57.0 + 113.0 * i.z;
    return mix(
        mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
            mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
        mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
            mix(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
}

// Fractal noise for clouds
float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < 3; i++) {
        value += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main()
{
    vec3 dir = normalize(PixelPosition);

    // Sample base cubemap
    vec3 color = textureCube(s_texSkybox, dir).rgb;

    // Add procedural clouds (only in upper hemisphere)
    if (dir.y > 0.0) {
        // Project direction to cloud plane (xz plane at y=1)
        vec3 cloudPos = dir / max(dir.y, 0.1) * 50.0;
        cloudPos.y += 10.0; // Cloud layer height

        // Generate cloud density
        float cloudDensity = fbm(cloudPos * 0.05);
        cloudDensity = smoothstep(0.3, 0.7, cloudDensity); // Threshold for cloud appearance

        // Add detail layer
        float detail = fbm(cloudPos * 0.2);
        cloudDensity = mix(cloudDensity, detail, 0.3);

        // Cloud color (white with slight blue tint)
        vec3 cloudColor = vec3(0.95, 0.97, 1.0);

        // Blend clouds with sky
        float cloudFactor = cloudDensity * 0.6; // Reduce cloud intensity
        color = mix(color, cloudColor, cloudFactor);
    }

    gl_FragColor = vec4(color, 1.0);
}
