#pragma once

#include <Render/Particle/ParticleSystem.hxx>
#include <Math/Vector.hxx>
#include <Physics/Fluid.hxx>
#include <cstdint>

namespace Solstice::ThermalPlume {

struct ExhaustParticleConfig {
    uint32_t MaxParticles{5000};
    float SpawnRate{300.0f};
    float MaxDistance{80.0f};
    float Density{1.0f};

    Math::Vec3 NozzlePosition{0.0f, 2.0f, 0.0f};
    float NozzleRadius{0.15f};
    float ExhaustVelocity{8.0f};
    float TurbulenceIntensity{0.5f};

    float SizeMin{0.03f};
    float SizeMax{0.12f};
    float LifeMin{0.4f};
    float LifeMax{2.5f};
};

// Temperature-colored exhaust particle system for rocket plume visualization.
// Particles spawn at the nozzle exit, inherit fluid velocity from the NSSolver grid,
// and are colored by a white-hot -> orange -> red -> smoke gray temperature ramp.
class ExhaustParticleSystem : public Render::ParticleSystem {
public:
    explicit ExhaustParticleSystem(const ExhaustParticleConfig& Config);
    ~ExhaustParticleSystem() override = default;

    void SetFluidSimulation(Physics::FluidSimulation* Fluid) { m_Fluid = Fluid; }
    void SetNozzlePosition(const Math::Vec3& Pos) { m_NozzlePosition = Pos; }
    void SetNozzleRadius(float R) { m_NozzleRadius = R; }
    void SetExhaustVelocity(float V) { m_ExhaustVelocity = V; }
    void SetTurbulenceIntensity(float T) { m_TurbulenceIntensity = T; }
    void SetThrottle(float T) { m_Throttle = T; }
    void SetFiring(bool F) { m_Firing = F; }

    void KillAllParticles() { m_ActiveParticles = 0; }

protected:
    void SpawnParticle(const Math::Vec3& SpawnPos) override;
    void UpdateParticle(Render::Particle& P, float Dt) override;
    void BuildVertexData(
        const Render::Particle& P,
        std::vector<uint8_t>& Vertices,
        size_t& VertexOffset,
        const Math::Vec3& CameraRight,
        const Math::Vec3& CameraUp) override;
    bgfx::VertexLayout GetVertexLayout() const override;
    size_t GetVertexSize() const override;

private:
    struct ParticleVertex {
        float X, Y, Z;
        uint8_t R, G, B, A;
        float U, V;
    };

    static uint32_t Hash(uint32_t X, uint32_t Y, uint32_t Seed);
    static float RandFloat(uint32_t H);
    Math::Vec3 TemperatureToColor(float T) const;

    Physics::FluidSimulation* m_Fluid{nullptr};
    Math::Vec3 m_NozzlePosition{0.0f, 2.0f, 0.0f};
    float m_NozzleRadius{0.15f};
    float m_ExhaustVelocity{8.0f};
    float m_TurbulenceIntensity{0.5f};
    float m_Throttle{0.0f};
    bool m_Firing{false};
    uint32_t m_Seed{77777};
    float m_SizeMin{0.03f};
    float m_SizeMax{0.12f};
    float m_LifeMin{0.4f};
    float m_LifeMax{2.5f};

    static uint32_t s_SpawnCounter;
};

} // namespace Solstice::ThermalPlume
