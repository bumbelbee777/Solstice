$input TexCoord

/*
 * Post Processing Fragment Shader
 * ACES Tone Mapping, Vignette, Grading
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor,  0);
SAMPLER2D(s_texDepth,  1);
SAMPLER2D(s_texVelocity, 2);
SAMPLER2D(s_texVolumetric, 3);  // Volumetric lighting (god rays)
uniform vec4 u_hdrExposure; // x: exposure, yzw: unused
uniform vec4 u_motionBlurParams; // x: strength, y: sampleCount, z: depthScale, w: unused
uniform mat4 u_prevViewProj; // Previous frame view-projection matrix
uniform vec4 u_viewportSize; // x: width, y: height, zw: unused
uniform vec4 u_bloomParams; // x: threshold, y: intensity, z: radius, w: enabled
uniform vec4 u_godRayParams; // x: density, y: decay, z: exposure, w: enabled

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

// Fast log approximation for HDR (polynomial)
float fastLog(float x) {
    x = clamp(x, 0.1, 10.0);
    float t = (x - 0.1) / 9.9;
    // Polynomial approximation: log(x) ≈ (x-1) - (x-1)²/2 + (x-1)³/3 for small values
    // For our range, use simpler approximation
    return (x - 1.0) * 0.5; // Simplified for performance
}

// Fast exp approximation for HDR (polynomial)
float fastExp(float x) {
    x = clamp(x, -2.0, 2.0);
    // Polynomial approximation: exp(x) ≈ 1 + x + x²/2 + x³/6
    return 1.0 + x + x*x*0.5 + x*x*x*0.166667;
}

// Bloom extraction - extract bright pixels above threshold with smooth falloff
vec3 extractBrightness(vec3 Color, float Threshold) {
    float Brightness = dot(Color, vec3(0.2126, 0.7152, 0.0722));
    float Soft = Brightness - Threshold + 0.5;
    Soft = clamp(Soft, 0.0, 1.0);
    Soft = Soft * Soft; // Smooth quadratic falloff
    float Contribution = max(0.0, Brightness - Threshold) / max(Brightness, 0.001);
    return Color * Soft * Contribution;
}


void main()
{
    // Optimized post-processing for CPU/iGPU with HDR support
    // Sample base color once (HDR RGBA16F)
    vec3 color = texture2D(s_texColor, TexCoord).rgb;

    // 1. HDR Exposure adjustment
    float exposure = u_hdrExposure.x;
    if (exposure > 0.001) {
        color *= exposure;
    } else {
        color *= 1.25; // Default exposure fallback
    }

    // 2. Motion Blur (if enabled) - Optimized with reduced samples
    float motionBlurStrength = u_motionBlurParams.x;
    float sampleCount = u_motionBlurParams.y;
    float depthScale = u_motionBlurParams.z;

    if (motionBlurStrength > 0.001 && sampleCount > 0.5) {
        // Sample depth
        float depth = texture2D(s_texDepth, TexCoord).r;

        // Sample velocity from velocity buffer (if available)
        vec2 screenVel = vec2(0.0, 0.0);
        bool hasVelocity = false;

        // Try to sample velocity buffer
        vec2 velocitySample = texture2D(s_texVelocity, TexCoord).rg;
        if (length(velocitySample) > 0.001) {
            screenVel = velocitySample;
            hasVelocity = true;
        }

        if (hasVelocity) {
            // Calculate velocity magnitude
            float velMag = length(screenVel);

            // Apply depth-aware scaling (reduce blur for near objects)
            float depthFactor = 1.0 - clamp(depth * depthScale, 0.0, 1.0);
            velMag *= depthFactor * motionBlurStrength;

            // Optimized motion blur with fewer samples
            if (velMag > 0.001) {
                vec2 velDir = normalize(screenVel);
                vec3 blurColor = color;
                float totalWeight = 1.0;

                // Reduced max samples for performance
                #define MAX_MOTION_BLUR_SAMPLES 8
                int numSamples = min(int(sampleCount), MAX_MOTION_BLUR_SAMPLES);

                // Optimized loop with early exit
                for (int i = 1; i <= MAX_MOTION_BLUR_SAMPLES && i <= numSamples; i++) {
                    float t = float(i) / float(numSamples);
                    vec2 offset = velDir * velMag * t;
                    vec2 sampleUV = TexCoord + offset / u_viewportSize.xy;
                    sampleUV = clamp(sampleUV, 0.0, 1.0);

                    float weight = 1.0 - t;
                    blurColor += texture2D(s_texColor, sampleUV).rgb * weight;
                    totalWeight += weight;
                }

                color = blurColor / totalWeight;
            }
        }
    }

    // 3. Optimized sharpening (removed - too expensive, minimal visual impact)
    // Sharpening removed for performance

    // 4. Vignette - removed for better image quality
    // Vignette was causing issues at corners, removed entirely

    // 5. Optimized grading (combined operations)
    // Contrast + Saturation in one pass
    vec3 gray = vec3_splat(dot(color, vec3(0.299, 0.587, 0.114)));
    color = mix(gray, color, 1.2); // Saturation
    color = pow(color, vec3_splat(1.08)); // Subtle contrast

    // 5b. Bloom - extract and blur bright pixels (optimized with 3-tap)
    if (u_bloomParams.w > 0.5) {
        float BloomThreshold = u_bloomParams.x;
        float BloomIntensity = u_bloomParams.y;
        float BloomRadius = u_bloomParams.z;

        // 3-tap bloom blur (reduced from 5-tap for performance)
        float Offset = BloomRadius / 1280.0;
        vec3 Bloom = extractBrightness(color, BloomThreshold);
        Bloom += extractBrightness(texture2D(s_texColor, TexCoord + vec2(Offset, 0.0)).rgb, BloomThreshold);
        Bloom += extractBrightness(texture2D(s_texColor, TexCoord - vec2(Offset, 0.0)).rgb, BloomThreshold);
        Bloom *= 0.333; // Average (1/3)

        // Additive blend
        color += Bloom * BloomIntensity;
    }

    // 5c. God Rays (volumetric lighting from CPU-traced texture)
    if (u_godRayParams.w > 0.5) {
        vec3 Volumetric = texture2D(s_texVolumetric, TexCoord).rgb;
        float GodRayExposure = u_godRayParams.z;

        // Additive blend with exposure control
        color += Volumetric * GodRayExposure;
    }

    // 6. Tone Mapping (ACES - already optimized)
    vec3 mapped = aces(color);

    // 7. Gamma correction
    mapped = pow(mapped, vec3_splat(1.0/2.2));

    // 8. Cheap dither (reduced noise, disabled for performance)
    // Dithering removed for performance - minimal visual impact

    gl_FragColor = vec4(mapped, 1.0);
}
