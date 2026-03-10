#pragma once

#include <Solstice.hxx>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <bgfx/bgfx.h>
#include <vector>
#include <cstdint>
#include <memory>

namespace Solstice::Render {

// Base particle structure - derived classes can extend this
struct Particle {
    Math::Vec3 Position;
    Math::Vec3 Velocity;
    float Size;
    float Life;
    float MaxLife;
    float Alpha;
    float Rotation;
    float RotationSpeed;
    float StartSize;
    float EndSize;
    float StartAlpha;
    float EndAlpha;

    Particle()
        : Position(0, 0, 0)
        , Velocity(0, 0, 0)
        , Size(1.0f)
        , Life(0.0f)
        , MaxLife(1.0f)
        , Alpha(1.0f)
        , Rotation(0.0f)
        , RotationSpeed(0.0f)
        , StartSize(1.0f)
        , EndSize(1.0f)
        , StartAlpha(1.0f)
        , EndAlpha(0.0f) {}
};

// Base class for particle systems
// Provides common functionality while allowing customization through virtual methods
class SOLSTICE_API ParticleSystem {
public:
    ParticleSystem();
    virtual ~ParticleSystem();

    // Initialize particle system
    // maxParticles: Maximum number of particles
    // vertexShader: Vertex shader name (e.g., "vs_particle.bin")
    // fragmentShader: Fragment shader name (e.g., "fs_particle.bin")
    void Initialize(uint32_t maxParticles, const std::string& vertexShader, const std::string& fragmentShader);

    // Shutdown and cleanup
    void Shutdown();

    // Update particle system
    // dt: Delta time
    // cameraPos: Camera position for culling and spawning
    void Update(float dt, const Math::Vec3& cameraPos);

    // Update with wind direction (optional, for particle systems that support wind)
    // Default implementation just calls Update(Dt, CameraPos)
    // Derived classes can override to use wind direction
    virtual void UpdateWithWind(float Dt, const Math::Vec3& CameraPos, const Math::Vec3& WindDirection) {
        (void)WindDirection; // Unused in base implementation
        Update(Dt, CameraPos);
    }

    // Set wind direction (optional, for particle systems that support wind)
    // Default implementation does nothing
    virtual void SetWindDirection(const Math::Vec3& Direction) { (void)Direction; }

    // Set wind strength (optional, for particle systems that support wind)
    // Default implementation does nothing
    virtual void SetWindStrength(float Strength) { (void)Strength; }

    // Render particles
    // viewId: BGFX view ID
    // viewProj: View-projection matrix
    // cameraRight/cameraUp: Camera basis for billboards (world space)
    void Render(bgfx::ViewId viewId, const Math::Matrix4& viewProj, const Math::Vec3& cameraRight, const Math::Vec3& cameraUp);

    // Configuration
    void SetSpawnRate(float rate) { m_SpawnRate = rate; }
    void SetMaxDistance(float distance) { m_MaxDistance = distance; }
    void SetDensity(float density) { m_Density = density; }

    float GetSpawnRate() const { return m_SpawnRate; }
    float GetMaxDistance() const { return m_MaxDistance; }
    float GetDensity() const { return m_Density; }
    uint32_t GetActiveParticleCount() const { return m_ActiveParticles; }
    uint32_t GetMaxParticles() const { return m_MaxParticles; }
    uint32_t GetSpawnBudgetPerFrame() const { return m_SpawnBudgetPerFrame; }

    void SetSpawnBudgetPerFrame(uint32_t budget) { m_SpawnBudgetPerFrame = budget; }

    // Global performance scaling (applies to all particle systems)
    static void SetPerfScalingEnabled(bool enabled);
    static void SetPerfScalingTargetFps(float fps);
    static void SetPerfScalingMinScale(float scale);
    static void SetPerfScalingWindow(int frames);

protected:
    // Virtual methods for customization - must be implemented by derived classes
    // Spawn a new particle at the given position
    virtual void SpawnParticle(const Math::Vec3& spawnPos) = 0;

    // Update a single particle
    // particle: Reference to particle to update
    // dt: Delta time
    virtual void UpdateParticle(Particle& particle, float dt) = 0;

    // Build vertex data for a particle
    // particle: Particle to build vertices for
    // vertices: Output vector to append vertices to
    // Each particle should generate 4 vertices (a quad) for billboard rendering
    virtual void BuildVertexData(
        const Particle& particle,
        std::vector<uint8_t>& vertices,
        size_t& vertexOffset,
        const Math::Vec3& cameraRight,
        const Math::Vec3& cameraUp) = 0;

    // Get vertex layout for particles
    virtual bgfx::VertexLayout GetVertexLayout() const = 0;

    // Get size of one vertex in bytes
    virtual size_t GetVertexSize() const = 0;

    // Common state
    std::vector<Particle> m_Particles;
    uint32_t m_MaxParticles;
    uint32_t m_ActiveParticles;

    float m_SpawnTimer;
    float m_SpawnRate; // Particles per second
    float m_MaxDistance; // Maximum distance from camera before culling
    float m_Density; // Density multiplier (affects spawn rate)
    uint32_t m_SpawnBudgetPerFrame;

    // Rendering
    bgfx::ProgramHandle m_Program;
    bgfx::DynamicVertexBufferHandle m_VertexBuffer;
    bool m_Initialized;
    std::vector<uint8_t> m_VertexScratch;

private:
    // Global performance scaling state
    static bool s_PerfScalingEnabled;
    static float s_PerfScalingTargetFps;
    static float s_PerfScalingMinScale;
    static int s_PerfScalingWindow;
};

} // namespace Solstice::Render
