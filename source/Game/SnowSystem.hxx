#pragma once

#include "../Solstice.hxx"
#include "../Math/Vector.hxx"
#include "../Arzachel/Seed.hxx"
#include <cstdint>

namespace Solstice::Game {

/**
 * Snow system for calculating snow depth and movement modifiers
 * Supports variable depth based on position (drifts, packed areas)
 */
class SOLSTICE_API SnowSystem {
public:
    SnowSystem();
    ~SnowSystem() = default;

    /**
     * Calculate snow depth at a given position
     * @param position World position (X, Y, Z)
     * @return Snow depth in meters (0.0 = no snow, 0.5 = ankle-deep, 1.0+ = deep snow)
     */
    float CalculateSnowDepth(const Math::Vec3& position) const;

    /**
     * Get snow resistance multiplier based on depth
     * Higher resistance = more velocity damping
     * @param depth Snow depth in meters
     * @return Resistance multiplier (0.0 = no resistance, 1.0 = maximum resistance)
     */
    float GetSnowResistance(float depth) const;

    /**
     * Get snow friction multiplier based on depth
     * @param depth Snow depth in meters
     * @return Friction multiplier (1.0 = normal friction, >1.0 = more friction)
     */
    float GetSnowFriction(float depth) const;

    /**
     * Get sink offset based on depth and velocity
     * Player sinks more when stationary, less when moving
     * @param depth Snow depth in meters
     * @param velocity Current horizontal velocity magnitude
     * @return Sink offset in meters (how much to lower player position)
     */
    float GetSinkOffset(float depth, float velocity) const;

    /**
     * Configure snow system parameters
     */
    void SetBaseDepth(float baseDepth) { m_BaseDepth = baseDepth; }
    void SetDepthVariation(float variation) { m_DepthVariation = variation; }
    void SetMaxDepth(float maxDepth) { m_MaxDepth = maxDepth; }
    void SetSeed(uint32_t seed) { m_Seed = Arzachel::Seed(seed); }

    float GetBaseDepth() const { return m_BaseDepth; }
    float GetDepthVariation() const { return m_DepthVariation; }
    float GetMaxDepth() const { return m_MaxDepth; }

private:
    float m_BaseDepth{0.3f};           // Base snow depth everywhere (meters)
    float m_DepthVariation{0.4f};      // Variation for drifts (meters)
    float m_MaxDepth{1.5f};            // Maximum snow depth (meters)
    Arzachel::Seed m_Seed{40000};      // Seed for noise generation

    // Movement modifier parameters
    static constexpr float m_ShallowDepth{0.2f};    // Shallow snow threshold
    static constexpr float m_MediumDepth{0.5f};    // Medium snow threshold
    static constexpr float m_ResistanceFactor{0.6f}; // Maximum resistance at max depth
    static constexpr float m_FrictionMultiplier{2.0f}; // Friction increase per meter of depth
    static constexpr float m_SinkFactor{0.3f};      // Sink amount factor
    static constexpr float m_VelocitySinkReduction{0.5f}; // How much velocity reduces sinking
};

} // namespace Solstice::Game

