#include "SnowSystem.hxx"
#include "../Arzachel/ProceduralTexture.hxx"
#include <algorithm>
#include <cmath>

namespace Solstice::Game {

SnowSystem::SnowSystem() {
}

float SnowSystem::CalculateSnowDepth(const Math::Vec3& position) const {
    // Use Perlin noise to create variable depth (drifts, packed areas)
    // Scale position to reasonable noise coordinates
    float noiseScale = 0.1f; // Adjust for drift size
    float noiseX = position.x * noiseScale;
    float noiseZ = position.z * noiseScale;

    // Generate noise value in range [-1, 1]
    float noise = Arzachel::ProceduralTexture::PerlinNoise2D(
        noiseX, noiseZ, 4, 1.0f, m_Seed);

    // Convert noise to [0, 1] range
    float normalizedNoise = (noise + 1.0f) * 0.5f;

    // Calculate depth: base + variation based on noise
    float depth = m_BaseDepth + normalizedNoise * m_DepthVariation;

    // Clamp to maximum depth
    return std::min(depth, m_MaxDepth);
}

float SnowSystem::GetSnowResistance(float depth) const {
    // Resistance increases with depth
    // Shallow snow (0-0.2m): Minimal effect
    // Medium snow (0.2-0.5m): Moderate resistance
    // Deep snow (0.5m+): High resistance

    if (depth < m_ShallowDepth) {
        // Shallow snow: minimal resistance
        float factor = depth / m_ShallowDepth;
        return factor * 0.1f; // 0-10% resistance
    } else if (depth < m_MediumDepth) {
        // Medium snow: moderate resistance
        float factor = (depth - m_ShallowDepth) / (m_MediumDepth - m_ShallowDepth);
        return 0.1f + factor * 0.3f; // 10-40% resistance
    } else {
        // Deep snow: high resistance
        float factor = std::min(1.0f, (depth - m_MediumDepth) / (m_MaxDepth - m_MediumDepth));
        return 0.4f + factor * m_ResistanceFactor; // 40-100% resistance
    }
}

float SnowSystem::GetSnowFriction(float depth) const {
    // Friction increases with depth
    // Formula: friction = 1.0 + depth * frictionMultiplier
    return 1.0f + depth * m_FrictionMultiplier;
}

float SnowSystem::GetSinkOffset(float depth, float velocity) const {
    // Player sinks more when stationary, less when moving
    // Velocity factor: higher velocity = less sinking
    float velocityFactor = std::min(1.0f, velocity / 5.0f); // Normalize velocity (5 m/s = no sinking)
    float sinkReduction = velocityFactor * m_VelocitySinkReduction;

    // Calculate sink offset
    float sinkOffset = depth * m_SinkFactor * (1.0f - sinkReduction);

    return sinkOffset;
}

} // namespace Solstice::Game

