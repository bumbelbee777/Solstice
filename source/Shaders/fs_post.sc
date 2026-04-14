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
SAMPLERCUBE(s_texReflectionProbe, 4);
SAMPLER2D(s_texHistory, 5);
uniform vec4 u_hdrExposure; // x: exposure, yzw: unused
uniform vec4 u_motionBlurParams; // x: strength, y: sampleCount, z: depthScale, w: unused
uniform mat4 u_prevViewProj; // Previous frame view-projection matrix
uniform vec4 u_viewportSize; // x: width, y: height, zw: unused
uniform vec4 u_bloomParams; // x: threshold, y: intensity, z: radius, w: enabled
uniform vec4 u_godRayParams; // x: density, y: decay, z: exposure, w: enabled
uniform vec4 u_reflectionParams; // x: intensity, y: maxSteps, z: thickness, w: stride
uniform mat4 u_reflectionViewProj;
uniform mat4 u_reflectionInvViewProj;
uniform vec4 u_cameraPos;
uniform vec4 u_taaParams; // x: blend, y: clampStrength, z: sharpenAmount, w: historyValid
uniform vec4 u_taaJitter; // xy: current jitter ndc, zw: previous jitter ndc

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

vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = mul(u_reflectionInvViewProj, clip);
    world.xyz /= max(world.w, 0.0001);
    return world.xyz;
}

vec3 computeNormalFromDepth(vec2 uv, float depth) {
    vec3 p = reconstructWorldPos(uv, depth);
    vec3 dx = dFdx(p);
    vec3 dy = dFdy(p);
    vec3 n = normalize(cross(dx, dy));
    return n;
}

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
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

    // 2. Temporal AA (balanced quality/cost)
    float taaBlend = clamp(u_taaParams.x, 0.0, 0.95);
    float taaClampStrength = max(u_taaParams.y, 0.01);
    float taaSharpen = clamp(u_taaParams.z, 0.0, 0.5);
    bool taaHasHistory = u_taaParams.w > 0.5;

    // Reconstruct motion in UV space.
    float depthAtPixel = texture2D(s_texDepth, TexCoord).r;
    vec2 screenVelUv = texture2D(s_texVelocity, TexCoord).rg;
    bool hasVelocity = length(screenVelUv) > 0.0002;
    if (!hasVelocity && depthAtPixel < 0.999) {
        vec3 worldPos = reconstructWorldPos(TexCoord, depthAtPixel);
        vec4 prevClip = mul(u_prevViewProj, vec4(worldPos, 1.0));
        if (prevClip.w > 0.0001) {
            vec2 prevUv = prevClip.xy / prevClip.w * 0.5 + 0.5;
            screenVelUv = TexCoord - prevUv;
            hasVelocity = length(screenVelUv) > 0.0002;
        }
    }

    if (taaHasHistory) {
        vec2 jitterDeltaUv = (u_taaJitter.xy - u_taaJitter.zw) * 0.5;
        vec2 historyUv = clamp(TexCoord - screenVelUv - jitterDeltaUv, 0.0, 1.0);
        vec3 historyColor = texture2D(s_texHistory, historyUv).rgb;

        // Neighborhood clamp to reduce ghosting.
        vec2 texel = vec2(1.0, 1.0) / u_viewportSize.xy;
        vec3 n1 = texture2D(s_texColor, TexCoord + vec2(texel.x, 0.0)).rgb;
        vec3 n2 = texture2D(s_texColor, TexCoord - vec2(texel.x, 0.0)).rgb;
        vec3 n3 = texture2D(s_texColor, TexCoord + vec2(0.0, texel.y)).rgb;
        vec3 n4 = texture2D(s_texColor, TexCoord - vec2(0.0, texel.y)).rgb;
        vec3 minNeighbor = min(min(n1, n2), min(n3, n4));
        vec3 maxNeighbor = max(max(n1, n2), max(n3, n4));
        vec3 clampMin = min(minNeighbor, color) - vec3_splat(taaClampStrength * 0.05);
        vec3 clampMax = max(maxNeighbor, color) + vec3_splat(taaClampStrength * 0.05);
        historyColor = clamp(historyColor, clampMin, clampMax);

        float lumaDelta = abs(luminance(color) - luminance(historyColor));
        float historyWeight = taaBlend * (1.0 - clamp(lumaDelta * 4.0, 0.0, 1.0));
        historyWeight *= hasVelocity ? 1.0 : 0.8;
        color = mix(color, historyColor, historyWeight);

        // Tiny unsharp mask to recover detail after TAA.
        if (taaSharpen > 0.001) {
            vec3 center = color;
            vec3 blur = (n1 + n2 + n3 + n4) * 0.25;
            color = center + (center - blur) * taaSharpen;
        }
    }

    // 3. Motion Blur (if enabled) - Optimized with reduced samples
    float motionBlurStrength = u_motionBlurParams.x;
    float sampleCount = u_motionBlurParams.y;
    float depthScale = u_motionBlurParams.z;

    if (motionBlurStrength > 0.001 && sampleCount > 0.5) {
        if (hasVelocity) {
            // Calculate velocity magnitude
            float velMag = length(screenVelUv);

            // Apply depth-aware scaling (reduce blur for near objects)
            float depthFactor = 1.0 - clamp(depthAtPixel * depthScale, 0.0, 1.0);
            velMag *= depthFactor * motionBlurStrength;

            // Optimized motion blur with fewer samples
            if (velMag > 0.001) {
                vec2 velDir = normalize(screenVelUv);
                vec3 blurColor = color;
                float totalWeight = 1.0;

                // Reduced max samples for performance
                #define MAX_MOTION_BLUR_SAMPLES 8
                int numSamples = min(int(sampleCount), MAX_MOTION_BLUR_SAMPLES);

                // Optimized loop with early exit
                for (int i = 1; i <= MAX_MOTION_BLUR_SAMPLES && i <= numSamples; i++) {
                    float t = float(i) / float(numSamples);
                    vec2 offset = velDir * velMag * t;
                    vec2 sampleUV = TexCoord + offset;
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
        // Values are in half-precision, so they should be in reasonable range
        color += Volumetric * GodRayExposure;
    }

    // 5d. Screen-space reflections + probe fallback (cheap)
    float reflectionIntensity = u_reflectionParams.x;
    if (reflectionIntensity > 0.001) {
        float depth = texture2D(s_texDepth, TexCoord).r;
        if (depth < 0.999) {
            vec3 worldPos = reconstructWorldPos(TexCoord, depth);
            vec3 normal = computeNormalFromDepth(TexCoord, depth);
            float depthEdge = abs(dFdx(depth)) + abs(dFdy(depth));
            vec3 viewDir = normalize(u_cameraPos.xyz - worldPos);
            vec3 reflectDir = normalize(reflect(-viewDir, normal));

            vec3 ssrColor = vec3_splat(0.0);
            float hit = 0.0;
            const int MAX_STEPS = 24;
            int maxSteps = int(u_reflectionParams.y);
            float thickness = u_reflectionParams.z;
            float stride = u_reflectionParams.w;

            vec3 rayPos = worldPos + reflectDir * stride;
            for (int i = 0; i < MAX_STEPS; ++i) {
                if (i >= maxSteps) { break; }

                vec4 clip = mul(u_reflectionViewProj, vec4(rayPos, 1.0));
                if (clip.w <= 0.0001) { break; }
                vec2 uv = clip.xy / clip.w * 0.5 + 0.5;

                if (uv.x < 0.001 || uv.x > 0.999 || uv.y < 0.001 || uv.y > 0.999) { break; }

                float sampleDepth = texture2D(s_texDepth, uv).r;
                if (sampleDepth < 0.0001 || sampleDepth > 0.999) {
                    rayPos += reflectDir * stride;
                    continue;
                }

                float rayDepth = clip.z / clip.w;
                rayDepth = rayDepth * 0.5 + 0.5;
                float depthDelta = abs(sampleDepth - rayDepth);

                if (depthDelta < thickness) {
                    ssrColor = texture2D(s_texColor, uv).rgb;
                    hit = 1.0;
                    break;
                }

                rayPos += reflectDir * stride;
            }

            vec3 probeColor = textureCube(s_texReflectionProbe, reflectDir).rgb;
            vec3 reflectionColor = mix(probeColor, ssrColor, hit);

            float packedAlpha = texture2D(s_texColor, TexCoord).a;
            float roughness = clamp(packedAlpha, 0.05, 1.0);
            float isTransparent = step(packedAlpha, 0.9);
            roughness = mix(roughness, 1.0, isTransparent);

            float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);
            float edgeFade = 1.0 - smoothstep(0.002, 0.01, depthEdge);
            float reflectFactor = reflectionIntensity * (1.0 - roughness) * fresnel * edgeFade;
            color += reflectionColor * clamp(reflectFactor, 0.0, 1.0);
        }
    }

    // 6. Tone Mapping (ACES - already optimized)
    vec3 mapped = aces(color);

    // 7. Gamma correction
    mapped = pow(mapped, vec3_splat(1.0/2.2));

    // 8. Cheap dither (reduced noise, disabled for performance)
    // Dithering removed for performance - minimal visual impact

    gl_FragColor = vec4(mapped, 1.0);
}
