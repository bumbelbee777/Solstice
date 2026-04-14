#include "ExhaustParticleSystem.hxx"
#include <Core/Debug/Debug.hxx>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Solstice::ThermalPlume {

uint32_t ExhaustParticleSystem::s_SpawnCounter = 0;

ExhaustParticleSystem::ExhaustParticleSystem(const ExhaustParticleConfig& Config)
    : m_NozzlePosition(Config.NozzlePosition)
    , m_NozzleRadius(Config.NozzleRadius)
    , m_ExhaustVelocity(Config.ExhaustVelocity)
    , m_TurbulenceIntensity(Config.TurbulenceIntensity)
    , m_SizeMin(Config.SizeMin)
    , m_SizeMax(Config.SizeMax)
    , m_LifeMin(Config.LifeMin)
    , m_LifeMax(Config.LifeMax)
{
    Initialize(Config.MaxParticles, "vs_particle.bin", "fs_particle.bin");
    SetSpawnRate(Config.SpawnRate);
    SetMaxDistance(Config.MaxDistance);
    SetDensity(Config.Density);
}

uint32_t ExhaustParticleSystem::Hash(uint32_t X, uint32_t Y, uint32_t Seed) {
    uint32_t H = Seed;
    H ^= X * 0x9e3779b9u;
    H ^= Y * 0x9e3779b9u;
    H = (H << 13) | (H >> 19);
    H = H * 5 + 0xe6546b64u;
    return H;
}

float ExhaustParticleSystem::RandFloat(uint32_t H) {
    return static_cast<float>(H & 0x7FFFFFFF) / 2147483647.0f;
}

// White-hot (T~1) -> orange (T~0.6) -> red (T~0.35) -> smoke gray (T~0)
Math::Vec3 ExhaustParticleSystem::TemperatureToColor(float T) const {
    T = std::clamp(T, 0.0f, 1.0f);
    if (T > 0.85f) {
        float t = (T - 0.85f) / 0.15f;
        return Math::Vec3::Lerp(Math::Vec3(1.0f, 0.85f, 0.4f), Math::Vec3(1.0f, 1.0f, 0.95f), t);
    }
    if (T > 0.5f) {
        float t = (T - 0.5f) / 0.35f;
        return Math::Vec3::Lerp(Math::Vec3(1.0f, 0.4f, 0.05f), Math::Vec3(1.0f, 0.85f, 0.4f), t);
    }
    if (T > 0.25f) {
        float t = (T - 0.25f) / 0.25f;
        return Math::Vec3::Lerp(Math::Vec3(0.6f, 0.15f, 0.05f), Math::Vec3(1.0f, 0.4f, 0.05f), t);
    }
    float t = T / 0.25f;
    return Math::Vec3::Lerp(Math::Vec3(0.25f, 0.25f, 0.28f), Math::Vec3(0.6f, 0.15f, 0.05f), t);
}

void ExhaustParticleSystem::SpawnParticle(const Math::Vec3& /*SpawnPos*/) {
    if (m_ActiveParticles >= m_MaxParticles || !m_Firing || m_Throttle < 0.01f) {
        return;
    }

    uint32_t Counter = s_SpawnCounter++;
    uint32_t H1 = Hash(Counter, m_Seed, 0);
    uint32_t H2 = Hash(Counter, m_Seed + 1000, 1);
    uint32_t H3 = Hash(Counter, m_Seed + 2000, 2);
    uint32_t H4 = Hash(Counter, m_Seed + 3000, 3);

    float R1 = RandFloat(H1);
    float R2 = RandFloat(H2);
    float R3 = RandFloat(H3);
    float R4 = RandFloat(H4);

    // Spawn within nozzle exit disk (XZ plane at nozzle Y)
    float angle = R1 * 6.283185f;
    float radius = std::sqrt(R2) * m_NozzleRadius;
    Math::Vec3 spawnOffset(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);

    Render::Particle P;
    P.Position = m_NozzlePosition + spawnOffset;

    // Downward exhaust velocity + turbulent jitter
    float turb = m_TurbulenceIntensity * m_Throttle;
    float lateralJitterX = (R3 - 0.5f) * 2.0f * turb;
    float lateralJitterZ = (R4 - 0.5f) * 2.0f * turb;
    P.Velocity = Math::Vec3(lateralJitterX, -m_ExhaustVelocity * m_Throttle, lateralJitterZ);

    // If fluid sim is available, add the local fluid velocity for coupling
    if (m_Fluid) {
        Math::Vec3 fluidVel = m_Fluid->SampleVelocity(P.Position);
        P.Velocity += fluidVel * 2.0f;
    }

    P.Size = m_SizeMin + R1 * (m_SizeMax - m_SizeMin);
    P.MaxLife = m_LifeMin + R2 * (m_LifeMax - m_LifeMin);
    P.Life = P.MaxLife;
    P.StartSize = P.Size;
    P.EndSize = P.Size * 2.5f;
    P.StartAlpha = std::clamp(m_Throttle * 1.2f, 0.0f, 1.0f);
    P.EndAlpha = 0.0f;
    P.Alpha = P.StartAlpha;
    // Encode initial temperature in Rotation field (repurposed).
    // Hot at birth, cools over lifetime.
    P.Rotation = std::clamp(0.7f + m_Throttle * 0.3f, 0.0f, 1.0f);
    P.RotationSpeed = 0.0f;

    if (m_ActiveParticles < m_Particles.size()) {
        m_Particles[m_ActiveParticles] = P;
    } else {
        m_Particles.push_back(P);
    }
    m_ActiveParticles++;
}

void ExhaustParticleSystem::UpdateParticle(Render::Particle& P, float Dt) {
    // Gravity (reduced by buoyancy of hot gas)
    float temperature = P.Rotation;
    float buoyancyReduction = temperature * 0.8f;
    P.Velocity.y += (-9.81f * (1.0f - buoyancyReduction)) * Dt;

    // Fluid velocity coupling
    if (m_Fluid) {
        Math::Vec3 fluidVel = m_Fluid->SampleVelocity(P.Position);
        P.Velocity += fluidVel * Dt * 3.0f;
    }

    // Light drag
    P.Velocity = P.Velocity * (1.0f - 0.5f * Dt);

    P.Position += P.Velocity * Dt;
    P.Life -= Dt;

    float lifeRatio = P.MaxLife > 0.0f ? (P.Life / P.MaxLife) : 0.0f;
    float t = 1.0f - std::clamp(lifeRatio, 0.0f, 1.0f);
    float smooth = t * t * (3.0f - 2.0f * t);
    P.Size = P.StartSize + (P.EndSize - P.StartSize) * smooth;
    P.Alpha = P.StartAlpha + (P.EndAlpha - P.StartAlpha) * smooth;

    // Cool down temperature over lifetime
    P.Rotation = std::clamp(P.Rotation - Dt * 0.4f, 0.0f, 1.0f);
}

void ExhaustParticleSystem::BuildVertexData(
    const Render::Particle& P,
    std::vector<uint8_t>& Vertices,
    size_t& VertexOffset,
    const Math::Vec3& CameraRight,
    const Math::Vec3& CameraUp)
{
    float HalfSize = P.Size * 0.5f;
    float temperature = std::clamp(P.Rotation, 0.0f, 1.0f);
    Math::Vec3 color = TemperatureToColor(temperature);
    uint8_t R = static_cast<uint8_t>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f);
    uint8_t G = static_cast<uint8_t>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f);
    uint8_t B = static_cast<uint8_t>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f);
    uint8_t A = static_cast<uint8_t>(std::clamp(P.Alpha, 0.0f, 1.0f) * 255.0f);

    Math::Vec3 right = CameraRight * HalfSize;
    Math::Vec3 up = CameraUp * HalfSize;

    ParticleVertex Quad[4] = {
        {P.Position.x - right.x - up.x, P.Position.y - right.y - up.y, P.Position.z - right.z - up.z, R, G, B, A, 0.0f, 1.0f},
        {P.Position.x + right.x - up.x, P.Position.y + right.y - up.y, P.Position.z + right.z - up.z, R, G, B, A, 1.0f, 1.0f},
        {P.Position.x - right.x + up.x, P.Position.y - right.y + up.y, P.Position.z - right.z + up.z, R, G, B, A, 0.0f, 0.0f},
        {P.Position.x + right.x + up.x, P.Position.y + right.y + up.y, P.Position.z + right.z + up.z, R, G, B, A, 1.0f, 0.0f}
    };

    ParticleVertex Triangles[6] = {
        Quad[0], Quad[1], Quad[2],
        Quad[2], Quad[1], Quad[3]
    };

    constexpr size_t VSize = sizeof(ParticleVertex);
    for (int I = 0; I < 6; ++I) {
        if (VertexOffset + VSize <= Vertices.size()) {
            std::memcpy(Vertices.data() + VertexOffset, &Triangles[I], VSize);
            VertexOffset += VSize;
        }
    }
}

bgfx::VertexLayout ExhaustParticleSystem::GetVertexLayout() const {
    bgfx::VertexLayout Layout;
    Layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    return Layout;
}

size_t ExhaustParticleSystem::GetVertexSize() const {
    return sizeof(float) * 3 + sizeof(uint8_t) * 4 + sizeof(float) * 2;
}

} // namespace Solstice::ThermalPlume
