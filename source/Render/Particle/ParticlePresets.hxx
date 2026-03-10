#pragma once

#include <Solstice.hxx>
#include "ParticleSystem.hxx"
#include <Math/Vector.hxx>
#include <memory>
#include <type_traits>
#include <algorithm>

namespace Solstice::Render {

// Particle preset type enumeration
enum class ParticlePresetType {
    Snow,
    Fire,
    Electricity,
    Smoke,
    ToxicGas
};

// Base particle preset configuration with common parameters
struct ParticlePresetConfig {
    uint32_t MaxParticles{3000};
    float SpawnRate{100.0f}; // Particles per second
    float MaxDistance{60.0f};
    float Density{1.0f};
    Math::Vec3 WindDirection{0.0f, 0.0f, 0.0f};
    float WindStrength{0.0f};
    uint32_t Seed{54321};
    float BaseSpawnRadius{10.0f};
    float SpawnHeight{5.0f};
    float SizeMin{0.05f};
    float SizeMax{0.15f};
    float LifeMin{1.0f};
    float LifeMax{5.0f};
};

// Snow particle system configuration
struct SnowParticleConfig : public ParticlePresetConfig {
    SnowParticleConfig() {
        MaxParticles = 3000;
        SpawnRate = 100.0f;
        MaxDistance = 60.0f;
        Density = 1.0f;
        WindDirection = Math::Vec3(0.5f, 0.0f, 0.8f);
        WindStrength = 2.0f;
        Seed = 54321;
        BaseSpawnRadius = 30.0f;
        SpawnHeight = 20.0f;
        SizeMin = 0.05f;
        SizeMax = 0.15f;
        LifeMin = 10.0f;
        LifeMax = 30.0f;
    }

    float FallSpeed{2.0f};
};

// Fire particle system configuration
struct FireParticleConfig : public ParticlePresetConfig {
    FireParticleConfig() {
        MaxParticles = 500;
        SpawnRate = 50.0f;
        MaxDistance = 40.0f;
        Density = 1.0f;
        WindDirection = Math::Vec3(0.0f, 1.0f, 0.0f); // Upward
        WindStrength = 1.0f;
        Seed = 12345;
        BaseSpawnRadius = 2.0f;
        SpawnHeight = 0.5f;
        SizeMin = 0.1f;
        SizeMax = 0.3f;
        LifeMin = 0.5f;
        LifeMax = 2.0f;
    }

    float UpwardVelocity{2.0f};
    float Turbulence{0.5f};
    Math::Vec3 ColorStart{1.0f, 0.2f, 0.0f}; // Red
    Math::Vec3 ColorMid{1.0f, 0.6f, 0.0f};   // Orange
    Math::Vec3 ColorEnd{1.0f, 1.0f, 0.3f};   // Yellow
    float SizeGrowthRate{1.5f};
};

// Electricity particle system configuration
struct ElectricityParticleConfig : public ParticlePresetConfig {
    ElectricityParticleConfig() {
        MaxParticles = 200;
        SpawnRate = 100.0f;
        MaxDistance = 30.0f;
        Density = 1.0f;
        WindDirection = Math::Vec3(0.0f, 0.0f, 0.0f);
        WindStrength = 0.0f;
        Seed = 67890;
        BaseSpawnRadius = 1.0f;
        SpawnHeight = 0.1f;
        SizeMin = 0.05f;
        SizeMax = 0.15f;
        LifeMin = 0.1f;
        LifeMax = 0.3f;
    }

    float Speed{10.0f};
    float DirectionChangeRate{5.0f};
    Math::Vec3 Color{1.0f, 1.0f, 1.0f}; // White
    Math::Vec3 ColorAlt{0.5f, 0.7f, 1.0f}; // Light blue
    float Alpha{0.9f};
};

// Smoke particle system configuration
struct SmokeParticleConfig : public ParticlePresetConfig {
    SmokeParticleConfig() {
        MaxParticles = 300;
        SpawnRate = 30.0f;
        MaxDistance = 50.0f;
        Density = 1.0f;
        WindDirection = Math::Vec3(0.0f, 1.0f, 0.0f); // Upward
        WindStrength = 0.5f;
        Seed = 11111;
        BaseSpawnRadius = 3.0f;
        SpawnHeight = 1.0f;
        SizeMin = 0.2f;
        SizeMax = 0.5f;
        LifeMin = 3.0f;
        LifeMax = 8.0f;
    }

    float UpwardDrift{0.5f};
    Math::Vec3 ColorStart{0.1f, 0.1f, 0.1f}; // Dark gray
    Math::Vec3 ColorEnd{0.3f, 0.3f, 0.3f};   // Light gray
    float SizeGrowthRate{2.0f};
    float DensityOpacity{0.3f};
};

// Toxic gas particle system configuration
struct ToxicGasParticleConfig : public ParticlePresetConfig {
    ToxicGasParticleConfig() {
        MaxParticles = 400;
        SpawnRate = 20.0f;
        MaxDistance = 60.0f;
        Density = 1.0f;
        WindDirection = Math::Vec3(0.0f, 0.2f, 0.0f); // Slight upward
        WindStrength = 0.3f;
        Seed = 22222;
        BaseSpawnRadius = 5.0f;
        SpawnHeight = 0.5f;
        SizeMin = 0.3f;
        SizeMax = 0.6f;
        LifeMin = 5.0f;
        LifeMax = 15.0f;
    }

    float ExpansionRate{0.2f};
    Math::Vec3 ColorStart{0.2f, 0.4f, 0.1f}; // Dark green
    Math::Vec3 ColorMid{0.4f, 0.6f, 0.2f};   // Medium green
    Math::Vec3 ColorEnd{0.6f, 0.7f, 0.3f};  // Light green-yellow
    float DensityVisibility{0.4f};
};

// Internal snow particle system implementation
class SnowParticleSystemImpl : public ParticleSystem {
public:
    SnowParticleSystemImpl(const SnowParticleConfig& Config);
    ~SnowParticleSystemImpl() override = default;

    // Override base class methods for wind support
    void UpdateWithWind(float Dt, const Math::Vec3& CameraPos, const Math::Vec3& WindDirection) override;
    void SetWindDirection(const Math::Vec3& Direction) override { m_WindDirection = Direction; }
    void SetWindStrength(float Strength) override { m_WindStrength = Strength; }

protected:
    void SpawnParticle(const Math::Vec3& SpawnPos) override;
    void UpdateParticle(Particle& Particle, float Dt) override;
    void BuildVertexData(
        const Particle& Particle,
        std::vector<uint8_t>& Vertices,
        size_t& VertexOffset,
        const Math::Vec3& CameraRight,
        const Math::Vec3& CameraUp) override;
    bgfx::VertexLayout GetVertexLayout() const override;
    size_t GetVertexSize() const override;

private:
    Math::Vec3 m_WindDirection;
    float m_WindStrength;
    uint32_t m_Seed;
    static uint32_t s_SpawnCounter;

    // Config values
    float m_BaseSpawnRadius;
    float m_SpawnHeight;
    float m_FallSpeed;
    float m_SizeMin;
    float m_SizeMax;
    float m_LifeMin;
    float m_LifeMax;
};

// Unified particle system implementation that handles all preset types
class UnifiedParticleSystemImpl : public ParticleSystem {
public:
    UnifiedParticleSystemImpl(ParticlePresetType type, const ParticlePresetConfig& config);
    ~UnifiedParticleSystemImpl() override = default;

    // Override base class methods for wind support
    void UpdateWithWind(float Dt, const Math::Vec3& CameraPos, const Math::Vec3& WindDirection) override;
    void SetWindDirection(const Math::Vec3& Direction) override { m_WindDirection = Direction; }
    void SetWindStrength(float Strength) override { m_WindStrength = Strength; }

protected:
    void SpawnParticle(const Math::Vec3& SpawnPos) override;
    void UpdateParticle(Particle& Particle, float Dt) override;
    void BuildVertexData(
        const Particle& Particle,
        std::vector<uint8_t>& Vertices,
        size_t& VertexOffset,
        const Math::Vec3& CameraRight,
        const Math::Vec3& CameraUp) override;
    bgfx::VertexLayout GetVertexLayout() const override;
    size_t GetVertexSize() const override;

private:
    ParticlePresetType m_Type;
    Math::Vec3 m_WindDirection;
    float m_WindStrength;
    uint32_t m_Seed;
    static uint32_t s_SpawnCounter;

    // Preset-specific data
    // Use std::aligned_storage to avoid union default construction issues with non-trivial types
    struct SnowData { float FallSpeed; };
    struct FireData {
        float UpwardVelocity;
        float Turbulence;
        Math::Vec3 ColorStart;
        Math::Vec3 ColorMid;
        Math::Vec3 ColorEnd;
        float SizeGrowthRate;
    };
    struct ElectricityData {
        float Speed;
        float DirectionChangeRate;
        Math::Vec3 Color;
        Math::Vec3 ColorAlt;
        float Alpha;
    };
    struct SmokeData {
        float UpwardDrift;
        Math::Vec3 ColorStart;
        Math::Vec3 ColorEnd;
        float SizeGrowthRate;
        float DensityOpacity;
    };
    struct ToxicGasData {
        float ExpansionRate;
        Math::Vec3 ColorStart;
        Math::Vec3 ColorMid;
        Math::Vec3 ColorEnd;
        float DensityVisibility;
    };

    // Use the largest struct size for aligned storage
    static constexpr size_t MaxPresetDataSize = std::max({sizeof(SnowData), sizeof(FireData),
                                                           sizeof(ElectricityData), sizeof(SmokeData),
                                                           sizeof(ToxicGasData)});
    static constexpr size_t MaxPresetDataAlign = std::max({alignof(SnowData), alignof(FireData),
                                                            alignof(ElectricityData), alignof(SmokeData),
                                                            alignof(ToxicGasData)});
    std::aligned_storage_t<MaxPresetDataSize, MaxPresetDataAlign> m_PresetDataStorage;

    // Helper to get typed pointer to preset data
    template<typename T> T* GetPresetData() { return reinterpret_cast<T*>(&m_PresetDataStorage); }
    template<typename T> const T* GetPresetData() const { return reinterpret_cast<const T*>(&m_PresetDataStorage); }

    // Helper functions
    uint32_t LocalHash(uint32_t X, uint32_t Y, uint32_t Seed) const;
    float RandomFloat(uint32_t Hash) const;
    Math::Vec3 GetParticleColor(const Particle& Particle, float LifeRatio) const;
};

// Particle preset factory functions
namespace ParticlePresets {

    // Create a snow particle system with the given configuration
    SOLSTICE_API std::unique_ptr<ParticleSystem> CreateSnowParticleSystem(const SnowParticleConfig& Config = SnowParticleConfig());

    // Create a fire particle system with the given configuration
    SOLSTICE_API std::unique_ptr<ParticleSystem> CreateFireParticleSystem(const FireParticleConfig& Config = FireParticleConfig());

    // Create an electricity particle system with the given configuration
    SOLSTICE_API std::unique_ptr<ParticleSystem> CreateElectricityParticleSystem(const ElectricityParticleConfig& Config = ElectricityParticleConfig());

    // Create a smoke particle system with the given configuration
    SOLSTICE_API std::unique_ptr<ParticleSystem> CreateSmokeParticleSystem(const SmokeParticleConfig& Config = SmokeParticleConfig());

    // Create a toxic gas particle system with the given configuration
    SOLSTICE_API std::unique_ptr<ParticleSystem> CreateToxicGasParticleSystem(const ToxicGasParticleConfig& Config = ToxicGasParticleConfig());

    // Unified factory function
    SOLSTICE_API std::unique_ptr<ParticleSystem> CreateParticleSystem(ParticlePresetType type, const ParticlePresetConfig& config);

} // namespace ParticlePresets

} // namespace Solstice::Render

