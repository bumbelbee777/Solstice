#include "ParticlePresets.hxx"
#include <Render/Assets/ShaderLoader.hxx>
#include <Core/Debug/Debug.hxx>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

namespace Solstice::Render {

uint32_t SnowParticleSystemImpl::s_SpawnCounter = 0;

SnowParticleSystemImpl::SnowParticleSystemImpl(const SnowParticleConfig& Config)
    : m_WindDirection(Config.WindDirection)
    , m_WindStrength(Config.WindStrength)
    , m_Seed(Config.Seed)
    , m_BaseSpawnRadius(Config.BaseSpawnRadius)
    , m_SpawnHeight(Config.SpawnHeight)
    , m_FallSpeed(Config.FallSpeed)
    , m_SizeMin(Config.SizeMin)
    , m_SizeMax(Config.SizeMax)
    , m_LifeMin(Config.LifeMin)
    , m_LifeMax(Config.LifeMax)
{
    // Initialize base class
    Initialize(Config.MaxParticles, "vs_particle.bin", "fs_particle.bin");
    SetSpawnRate(Config.SpawnRate);
    SetMaxDistance(Config.MaxDistance);
    SetDensity(Config.Density);
}

void SnowParticleSystemImpl::UpdateWithWind(float Dt, const Math::Vec3& CameraPos, const Math::Vec3& WindDirection) {
    m_WindDirection = WindDirection;
    // Call base class Update which handles spawning and culling
    Update(Dt, CameraPos);
}

void SnowParticleSystemImpl::SpawnParticle(const Math::Vec3& SpawnPos) {
    if (m_ActiveParticles >= m_MaxParticles) {
        return;
    }

    // Local hash function for deterministic randomization
    auto LocalHash = [](uint32_t X, uint32_t Y, uint32_t Seed) -> uint32_t {
        uint32_t H = Seed;
        H ^= X * 0x9e3779b9u;
        H ^= Y * 0x9e3779b9u;
        H = (H << 13) | (H >> 19);
        H = H * 5 + 0xe6546b64u;
        return H;
    };

    // Use multiple hash seeds for better randomization
    uint32_t Counter = s_SpawnCounter++;
    uint32_t Hash1 = LocalHash(Counter, m_Seed, 0);
    uint32_t Hash2 = LocalHash(Counter, m_Seed + 1000, 1);
    uint32_t Hash3 = LocalHash(Counter, m_Seed + 2000, 2);
    uint32_t Hash4 = LocalHash(Counter, m_Seed + 3000, 3);

    float RandX = static_cast<float>(Hash1 & 0x7FFFFFFF) / 2147483647.0f;
    float RandY = static_cast<float>(Hash2 & 0x7FFFFFFF) / 2147483647.0f;
    float RandZ = static_cast<float>(Hash3 & 0x7FFFFFFF) / 2147483647.0f;
    float RandW = static_cast<float>(Hash4 & 0x7FFFFFFF) / 2147483647.0f;

    Particle Particle;

    // Spawn in a volume around camera with varied radius
    float SpawnRadius = m_BaseSpawnRadius + (RandW - 0.5f) * 10.0f; // Vary radius ±5 units

    // Add random jitter to break up line patterns - increased jitter to reduce artifacts
    float JitterX = (RandX - 0.5f) * 2.0f; // Increased jitter for better randomization
    float JitterY = (RandY - 0.5f) * 2.0f;
    float JitterZ = (RandZ - 0.5f) * 2.0f;

    // Use different random values for position components to break up patterns
    float PosRandX = static_cast<float>((Hash1 ^ Hash3) & 0x7FFFFFFF) / 2147483647.0f;
    float PosRandY = static_cast<float>((Hash2 ^ Hash4) & 0x7FFFFFFF) / 2147483647.0f;
    float PosRandZ = static_cast<float>((Hash3 ^ Hash1) & 0x7FFFFFFF) / 2147483647.0f;

    Particle.Position = SpawnPos + Math::Vec3(
        (PosRandX - 0.5f) * SpawnRadius * 2.0f + JitterX,
        PosRandY * m_SpawnHeight + 5.0f + JitterY,
        (PosRandZ - 0.5f) * SpawnRadius * 2.0f + JitterZ
    );

    // Initial velocity (falling with wind) - use different random values for more variation
    float VelRandX = static_cast<float>((Hash1 ^ Hash2) & 0x7FFFFFFF) / 2147483647.0f;
    float VelRandY = static_cast<float>((Hash2 ^ Hash3) & 0x7FFFFFFF) / 2147483647.0f;
    float VelRandZ = static_cast<float>((Hash3 ^ Hash4) & 0x7FFFFFFF) / 2147483647.0f;

    Particle.Velocity = Math::Vec3(
        m_WindDirection.x * m_WindStrength * (VelRandX - 0.5f) * 0.5f,
        -m_FallSpeed - VelRandY * 1.0f, // Falling speed
        m_WindDirection.z * m_WindStrength * (VelRandZ - 0.5f) * 0.5f
    );

    Particle.Size = m_SizeMin + RandX * (m_SizeMax - m_SizeMin);
    Particle.MaxLife = m_LifeMin + RandY * (m_LifeMax - m_LifeMin);
    Particle.Life = Particle.MaxLife;
    Particle.StartSize = Particle.Size;
    Particle.EndSize = Particle.Size;
    Particle.StartAlpha = 1.0f;
    Particle.EndAlpha = 0.0f;
    Particle.Alpha = Particle.StartAlpha;
    Particle.Rotation = RandZ * 6.283185f;
    Particle.RotationSpeed = (RandW - 0.5f) * 1.2f;

    if (m_ActiveParticles < m_Particles.size()) {
        m_Particles[m_ActiveParticles] = Particle;
    } else {
        m_Particles.push_back(Particle);
    }
    m_ActiveParticles++;
}

void SnowParticleSystemImpl::UpdateParticle(Particle& Particle, float Dt) {
    // Apply wind force
    Particle.Velocity += m_WindDirection * m_WindStrength * Dt * 0.5f;

    // Apply gravity
    Particle.Velocity.y -= 9.8f * Dt * 0.3f; // Slower fall for snow

    // Update position
    Particle.Position += Particle.Velocity * Dt;

    // Update life
    Particle.Life -= Dt;
    float lifeRatio = Particle.MaxLife > 0.0f ? (Particle.Life / Particle.MaxLife) : 0.0f;
    float t = 1.0f - std::clamp(lifeRatio, 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t);
    Particle.Size = Particle.StartSize + (Particle.EndSize - Particle.StartSize) * t;
    Particle.Alpha = Particle.StartAlpha + (Particle.EndAlpha - Particle.StartAlpha) * t;
    Particle.Rotation += Particle.RotationSpeed * Dt;
}

void SnowParticleSystemImpl::BuildVertexData(
    const Particle& Particle,
    std::vector<uint8_t>& Vertices,
    size_t& VertexOffset,
    const Math::Vec3& CameraRight,
    const Math::Vec3& CameraUp) {
    // Vertex structure: Position (3 float), Color0 (4 uint8), TexCoord0 (2 float)
    // Total: 3*4 + 4*1 + 2*4 = 12 + 4 + 8 = 24 bytes per vertex

    struct ParticleVertex {
        float X, Y, Z;
        uint8_t R, G, B, A;
        float U, V;
    };

    float Size = Particle.Size;
    float HalfSize = Size * 0.5f;
    uint8_t Alpha = static_cast<uint8_t>(Particle.Alpha * 255.0f);
    uint8_t R = 0xFF, G = 0xFF, B = 0xFF; // White snow

    // Improved offset calculation to reduce line artifacts
    // Use a more complex hash based on particle position and life to create varied offsets
    uint32_t PosHash = static_cast<uint32_t>(Particle.Position.x * 100.0f) ^
                       static_cast<uint32_t>(Particle.Position.z * 100.0f) ^
                       static_cast<uint32_t>(Particle.Life * 10.0f);

    // Create more varied offsets to break up line patterns
    float OffsetX = static_cast<float>((PosHash & 0xFF) - 128) / 128.0f * (HalfSize * 0.4f);
    float OffsetZ = static_cast<float>(((PosHash >> 8) & 0xFF) - 128) / 128.0f * (HalfSize * 0.4f);
    float OffsetY = static_cast<float>(((PosHash >> 16) & 0xFF) - 128) / 128.0f * (HalfSize * 0.2f);

    float cosR = std::cos(Particle.Rotation);
    float sinR = std::sin(Particle.Rotation);
    Math::Vec3 rotatedRight = CameraRight * cosR - CameraUp * sinR;
    Math::Vec3 rotatedUp = CameraRight * sinR + CameraUp * cosR;
    Math::Vec3 right = rotatedRight * HalfSize;
    Math::Vec3 up = rotatedUp * HalfSize;
    Math::Vec3 centerOffset = rotatedRight * OffsetX + rotatedUp * OffsetY;
    Math::Vec3 center = Particle.Position + centerOffset;

    ParticleVertex Quad[4] = {
        {center.x - right.x - up.x, center.y - right.y - up.y, center.z - right.z - up.z, R, G, B, Alpha, 0.0f, 1.0f},
        {center.x + right.x - up.x, center.y + right.y - up.y, center.z + right.z - up.z, R, G, B, Alpha, 1.0f, 1.0f},
        {center.x - right.x + up.x, center.y - right.y + up.y, center.z - right.z + up.z, R, G, B, Alpha, 0.0f, 0.0f},
        {center.x + right.x + up.x, center.y + right.y + up.y, center.z + right.z + up.z, R, G, B, Alpha, 1.0f, 0.0f}
    };

    ParticleVertex Triangles[6] = {
        Quad[0], Quad[1], Quad[2],
        Quad[2], Quad[1], Quad[3]
    };

    // Copy vertices to buffer
    size_t VertexSize = sizeof(ParticleVertex);
    for (int I = 0; I < 6; ++I) {
        if (VertexOffset + VertexSize <= Vertices.size()) {
            std::memcpy(Vertices.data() + VertexOffset, &Triangles[I], VertexSize);
            VertexOffset += VertexSize;
        }
    }
}

bgfx::VertexLayout SnowParticleSystemImpl::GetVertexLayout() const {
    bgfx::VertexLayout Layout;
    Layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    return Layout;
}

size_t SnowParticleSystemImpl::GetVertexSize() const {
    return sizeof(float) * 3 + sizeof(uint8_t) * 4 + sizeof(float) * 2; // 24 bytes
}

// UnifiedParticleSystemImpl implementation
uint32_t UnifiedParticleSystemImpl::s_SpawnCounter = 0;

UnifiedParticleSystemImpl::UnifiedParticleSystemImpl(ParticlePresetType type, const ParticlePresetConfig& config)
    : m_Type(type)
    , m_WindDirection(config.WindDirection)
    , m_WindStrength(config.WindStrength)
    , m_Seed(config.Seed)
{
    // Initialize preset-specific data based on type
    // Use placement new to construct specific preset data structs in aligned storage
    switch (type) {
        case ParticlePresetType::Snow: {
            const auto& snowConfig = static_cast<const SnowParticleConfig&>(config);
            new (GetPresetData<SnowData>()) SnowData{snowConfig.FallSpeed};
            break;
        }
        case ParticlePresetType::Fire: {
            const auto& fireConfig = static_cast<const FireParticleConfig&>(config);
            FireData* fireData = GetPresetData<FireData>();
            new (fireData) FireData();
            fireData->UpwardVelocity = fireConfig.UpwardVelocity;
            fireData->Turbulence = fireConfig.Turbulence;
            fireData->ColorStart = fireConfig.ColorStart;
            fireData->ColorMid = fireConfig.ColorMid;
            fireData->ColorEnd = fireConfig.ColorEnd;
            fireData->SizeGrowthRate = fireConfig.SizeGrowthRate;
            break;
        }
        case ParticlePresetType::Electricity: {
            const auto& elecConfig = static_cast<const ElectricityParticleConfig&>(config);
            ElectricityData* elecData = GetPresetData<ElectricityData>();
            new (elecData) ElectricityData();
            elecData->Speed = elecConfig.Speed;
            elecData->DirectionChangeRate = elecConfig.DirectionChangeRate;
            elecData->Color = elecConfig.Color;
            elecData->ColorAlt = elecConfig.ColorAlt;
            elecData->Alpha = elecConfig.Alpha;
            break;
        }
        case ParticlePresetType::Smoke: {
            const auto& smokeConfig = static_cast<const SmokeParticleConfig&>(config);
            SmokeData* smokeData = GetPresetData<SmokeData>();
            new (smokeData) SmokeData();
            smokeData->UpwardDrift = smokeConfig.UpwardDrift;
            smokeData->ColorStart = smokeConfig.ColorStart;
            smokeData->ColorEnd = smokeConfig.ColorEnd;
            smokeData->SizeGrowthRate = smokeConfig.SizeGrowthRate;
            smokeData->DensityOpacity = smokeConfig.DensityOpacity;
            break;
        }
        case ParticlePresetType::ToxicGas: {
            const auto& gasConfig = static_cast<const ToxicGasParticleConfig&>(config);
            ToxicGasData* gasData = GetPresetData<ToxicGasData>();
            new (gasData) ToxicGasData();
            gasData->ExpansionRate = gasConfig.ExpansionRate;
            gasData->ColorStart = gasConfig.ColorStart;
            gasData->ColorMid = gasConfig.ColorMid;
            gasData->ColorEnd = gasConfig.ColorEnd;
            gasData->DensityVisibility = gasConfig.DensityVisibility;
            break;
        }
    }

    // Initialize base class
    Initialize(config.MaxParticles, "vs_particle.bin", "fs_particle.bin");
    SetSpawnRate(config.SpawnRate);
    SetMaxDistance(config.MaxDistance);
    SetDensity(config.Density);
}

void UnifiedParticleSystemImpl::UpdateWithWind(float Dt, const Math::Vec3& CameraPos, const Math::Vec3& WindDirection) {
    m_WindDirection = WindDirection;
    Update(Dt, CameraPos);
}

uint32_t UnifiedParticleSystemImpl::LocalHash(uint32_t X, uint32_t Y, uint32_t Seed) const {
    uint32_t H = Seed;
    H ^= X * 0x9e3779b9u;
    H ^= Y * 0x9e3779b9u;
    H = (H << 13) | (H >> 19);
    H = H * 5 + 0xe6546b64u;
    return H;
}

float UnifiedParticleSystemImpl::RandomFloat(uint32_t Hash) const {
    return static_cast<float>(Hash & 0x7FFFFFFF) / 2147483647.0f;
}

Math::Vec3 UnifiedParticleSystemImpl::GetParticleColor(const Particle& Particle, float LifeRatio) const {
    switch (m_Type) {
        case ParticlePresetType::Fire: {
            const FireData* fireData = GetPresetData<FireData>();
            if (LifeRatio > 0.5f) {
                float t = (LifeRatio - 0.5f) * 2.0f;
                return Math::Vec3::Lerp(fireData->ColorMid, fireData->ColorEnd, t);
            } else {
                float t = LifeRatio * 2.0f;
                return Math::Vec3::Lerp(fireData->ColorStart, fireData->ColorMid, t);
            }
        }
        case ParticlePresetType::Smoke: {
            const SmokeData* smokeData = GetPresetData<SmokeData>();
            return Math::Vec3::Lerp(smokeData->ColorStart, smokeData->ColorEnd, 1.0f - LifeRatio);
        }
        case ParticlePresetType::ToxicGas: {
            const ToxicGasData* gasData = GetPresetData<ToxicGasData>();
            if (LifeRatio > 0.5f) {
                float t = (LifeRatio - 0.5f) * 2.0f;
                return Math::Vec3::Lerp(gasData->ColorMid, gasData->ColorEnd, t);
            } else {
                float t = LifeRatio * 2.0f;
                return Math::Vec3::Lerp(gasData->ColorStart, gasData->ColorMid, t);
            }
        }
        default:
            return Math::Vec3(1.0f, 1.0f, 1.0f); // White default
    }
}

void UnifiedParticleSystemImpl::SpawnParticle(const Math::Vec3& SpawnPos) {
    if (m_ActiveParticles >= m_MaxParticles) {
        return;
    }

    uint32_t Counter = s_SpawnCounter++;
    uint32_t Hash1 = LocalHash(Counter, m_Seed, 0);
    uint32_t Hash2 = LocalHash(Counter, m_Seed + 1000, 1);
    uint32_t Hash3 = LocalHash(Counter, m_Seed + 2000, 2);
    uint32_t Hash4 = LocalHash(Counter, m_Seed + 3000, 3);

    float RandX = RandomFloat(Hash1);
    float RandY = RandomFloat(Hash2);
    float RandZ = RandomFloat(Hash3);
    float RandW = RandomFloat(Hash4);

    Particle Particle;
    const ParticlePresetConfig* baseConfig = nullptr;

    // Get base config based on type (we'll use default values for now)
    float baseSpawnRadius = 10.0f;
    float spawnHeight = 5.0f;
    float sizeMin = 0.05f;
    float sizeMax = 0.15f;
    float lifeMin = 1.0f;
    float lifeMax = 5.0f;

    switch (m_Type) {
        case ParticlePresetType::Snow: {
            baseSpawnRadius = 30.0f;
            spawnHeight = 20.0f;
            sizeMin = 0.05f;
            sizeMax = 0.15f;
            lifeMin = 10.0f;
            lifeMax = 30.0f;
            break;
        }
        case ParticlePresetType::Fire: {
            baseSpawnRadius = 2.0f;
            spawnHeight = 0.5f;
            sizeMin = 0.1f;
            sizeMax = 0.3f;
            lifeMin = 0.5f;
            lifeMax = 2.0f;
            break;
        }
        case ParticlePresetType::Electricity: {
            baseSpawnRadius = 1.0f;
            spawnHeight = 0.1f;
            sizeMin = 0.05f;
            sizeMax = 0.15f;
            lifeMin = 0.1f;
            lifeMax = 0.3f;
            break;
        }
        case ParticlePresetType::Smoke: {
            baseSpawnRadius = 3.0f;
            spawnHeight = 1.0f;
            sizeMin = 0.2f;
            sizeMax = 0.5f;
            lifeMin = 3.0f;
            lifeMax = 8.0f;
            break;
        }
        case ParticlePresetType::ToxicGas: {
            baseSpawnRadius = 5.0f;
            spawnHeight = 0.5f;
            sizeMin = 0.3f;
            sizeMax = 0.6f;
            lifeMin = 5.0f;
            lifeMax = 15.0f;
            break;
        }
    }

    // Spawn position
    float SpawnRadius = baseSpawnRadius + (RandW - 0.5f) * 2.0f;
    Particle.Position = SpawnPos + Math::Vec3(
        (RandX - 0.5f) * SpawnRadius * 2.0f,
        RandY * spawnHeight,
        (RandZ - 0.5f) * SpawnRadius * 2.0f
    );

    // Initial velocity based on type
    switch (m_Type) {
        case ParticlePresetType::Snow: {
            const SnowData* snowData = GetPresetData<SnowData>();
            Particle.Velocity = Math::Vec3(
                m_WindDirection.x * m_WindStrength * (RandX - 0.5f) * 0.5f,
                -snowData->FallSpeed - RandY * 1.0f,
                m_WindDirection.z * m_WindStrength * (RandZ - 0.5f) * 0.5f
            );
            break;
        }
        case ParticlePresetType::Fire: {
            const FireData* fireData = GetPresetData<FireData>();
            float turbulenceX = (RandX - 0.5f) * fireData->Turbulence;
            float turbulenceZ = (RandZ - 0.5f) * fireData->Turbulence;
            Particle.Velocity = Math::Vec3(
                turbulenceX,
                fireData->UpwardVelocity + RandY * 0.5f,
                turbulenceZ
            );
            break;
        }
        case ParticlePresetType::Electricity: {
            const ElectricityData* elecData = GetPresetData<ElectricityData>();
            // Random direction with high speed
            Particle.Velocity = Math::Vec3(
                (RandX - 0.5f) * elecData->Speed,
                (RandY - 0.5f) * elecData->Speed,
                (RandZ - 0.5f) * elecData->Speed
            );
            break;
        }
        case ParticlePresetType::Smoke: {
            const SmokeData* smokeData = GetPresetData<SmokeData>();
            Particle.Velocity = Math::Vec3(
                (RandX - 0.5f) * 0.2f + m_WindDirection.x * m_WindStrength,
                smokeData->UpwardDrift + RandY * 0.3f,
                (RandZ - 0.5f) * 0.2f + m_WindDirection.z * m_WindStrength
            );
            break;
        }
        case ParticlePresetType::ToxicGas: {
            const ToxicGasData* gasData = GetPresetData<ToxicGasData>();
            Particle.Velocity = Math::Vec3(
                (RandX - 0.5f) * gasData->ExpansionRate + m_WindDirection.x * m_WindStrength,
                m_WindDirection.y * m_WindStrength + RandY * 0.1f,
                (RandZ - 0.5f) * gasData->ExpansionRate + m_WindDirection.z * m_WindStrength
            );
            break;
        }
    }

    Particle.Size = sizeMin + RandX * (sizeMax - sizeMin);
    Particle.MaxLife = lifeMin + RandY * (lifeMax - lifeMin);
    Particle.Life = Particle.MaxLife;
    const ElectricityData* elecData = (m_Type == ParticlePresetType::Electricity) ? GetPresetData<ElectricityData>() : nullptr;
    Particle.StartSize = Particle.Size;
    Particle.EndSize = Particle.Size;
    Particle.StartAlpha = 1.0f;
    Particle.EndAlpha = 0.0f;
    if (m_Type == ParticlePresetType::Electricity && elecData) {
        Particle.StartAlpha = elecData->Alpha;
    } else if (m_Type == ParticlePresetType::Smoke) {
        const SmokeData* smokeData = GetPresetData<SmokeData>();
        Particle.StartAlpha = std::clamp(smokeData->DensityOpacity + 0.15f, 0.15f, 0.9f);
    } else if (m_Type == ParticlePresetType::ToxicGas) {
        const ToxicGasData* gasData = GetPresetData<ToxicGasData>();
        Particle.StartAlpha = std::clamp(gasData->DensityVisibility + 0.1f, 0.1f, 0.85f);
    } else if (m_Type == ParticlePresetType::Fire) {
        Particle.StartAlpha = 0.95f;
    }

    if (m_Type == ParticlePresetType::Fire) {
        const FireData* fireData = GetPresetData<FireData>();
        Particle.EndSize = Particle.StartSize * (1.0f + fireData->SizeGrowthRate * 0.6f);
    } else if (m_Type == ParticlePresetType::Smoke) {
        const SmokeData* smokeData = GetPresetData<SmokeData>();
        Particle.EndSize = Particle.StartSize * (1.0f + smokeData->SizeGrowthRate);
    } else if (m_Type == ParticlePresetType::ToxicGas) {
        const ToxicGasData* gasData = GetPresetData<ToxicGasData>();
        Particle.EndSize = Particle.StartSize * (1.0f + gasData->ExpansionRate * 2.0f);
    }

    Particle.Alpha = Particle.StartAlpha;
    Particle.Rotation = RandZ * 6.283185f;
    Particle.RotationSpeed = (RandW - 0.5f) * 2.2f;

    if (m_ActiveParticles < m_Particles.size()) {
        m_Particles[m_ActiveParticles] = Particle;
    } else {
        m_Particles.push_back(Particle);
    }
    m_ActiveParticles++;
}

void UnifiedParticleSystemImpl::UpdateParticle(Particle& Particle, float Dt) {
    switch (m_Type) {
        case ParticlePresetType::Snow: {
            Particle.Velocity += m_WindDirection * m_WindStrength * Dt * 0.5f;
            Particle.Velocity.y -= 9.8f * Dt * 0.3f;
            break;
        }
        case ParticlePresetType::Fire: {
            const FireData* fireData = GetPresetData<FireData>();
            // Turbulence effect
            float turbulence = fireData->Turbulence * Dt;
            Particle.Velocity.x += (RandomFloat(static_cast<uint32_t>(Particle.Position.x * 100.0f)) - 0.5f) * turbulence;
            Particle.Velocity.z += (RandomFloat(static_cast<uint32_t>(Particle.Position.z * 100.0f)) - 0.5f) * turbulence;
            break;
        }
        case ParticlePresetType::Electricity: {
            const ElectricityData* elecData = GetPresetData<ElectricityData>();
            // Random direction changes
            if (RandomFloat(static_cast<uint32_t>(Particle.Life * 100.0f)) < elecData->DirectionChangeRate * Dt) {
                float randX = RandomFloat(static_cast<uint32_t>(Particle.Position.x * 100.0f));
                float randY = RandomFloat(static_cast<uint32_t>(Particle.Position.y * 100.0f));
                float randZ = RandomFloat(static_cast<uint32_t>(Particle.Position.z * 100.0f));
                Particle.Velocity = Math::Vec3(
                    (randX - 0.5f) * elecData->Speed,
                    (randY - 0.5f) * elecData->Speed,
                    (randZ - 0.5f) * elecData->Speed
                );
            }
            break;
        }
        case ParticlePresetType::Smoke: {
            Particle.Velocity += m_WindDirection * m_WindStrength * Dt * 0.3f;
            break;
        }
        case ParticlePresetType::ToxicGas: {
            Particle.Velocity += m_WindDirection * m_WindStrength * Dt * 0.2f;
            break;
        }
    }

    Particle.Position += Particle.Velocity * Dt;
    Particle.Life -= Dt;

    float lifeRatio = Particle.MaxLife > 0.0f ? (Particle.Life / Particle.MaxLife) : 0.0f;
    float t = 1.0f - std::clamp(lifeRatio, 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t);
    Particle.Size = Particle.StartSize + (Particle.EndSize - Particle.StartSize) * t;
    Particle.Alpha = Particle.StartAlpha + (Particle.EndAlpha - Particle.StartAlpha) * t;
    Particle.Rotation += Particle.RotationSpeed * Dt;
}

void UnifiedParticleSystemImpl::BuildVertexData(
    const Particle& Particle,
    std::vector<uint8_t>& Vertices,
    size_t& VertexOffset,
    const Math::Vec3& CameraRight,
    const Math::Vec3& CameraUp) {
    struct ParticleVertex {
        float X, Y, Z;
        uint8_t R, G, B, A;
        float U, V;
    };

    float Size = Particle.Size;
    float HalfSize = Size * 0.5f;
    uint8_t Alpha = static_cast<uint8_t>(Particle.Alpha * 255.0f);

    Math::Vec3 Color = GetParticleColor(Particle, Particle.Life / Particle.MaxLife);
    if (m_Type == ParticlePresetType::Electricity) {
        const ElectricityData* elecData = GetPresetData<ElectricityData>();
        // Alternate between white and blue for electricity
        float lifeHash = static_cast<float>(static_cast<uint32_t>(Particle.Life * 10.0f) % 2);
        Color = (lifeHash < 1.0f) ? elecData->Color : elecData->ColorAlt;
    }

    uint8_t R = static_cast<uint8_t>(Color.x * 255.0f);
    uint8_t G = static_cast<uint8_t>(Color.y * 255.0f);
    uint8_t B = static_cast<uint8_t>(Color.z * 255.0f);

    uint32_t PosHash = static_cast<uint32_t>(Particle.Position.x * 97.0f) ^
                       static_cast<uint32_t>(Particle.Position.z * 83.0f) ^
                       static_cast<uint32_t>(Particle.MaxLife * 31.0f);
    float OffsetX = static_cast<float>((PosHash & 0xFF) - 128) / 128.0f * (HalfSize * 0.15f);
    float OffsetY = static_cast<float>(((PosHash >> 8) & 0xFF) - 128) / 128.0f * (HalfSize * 0.15f);

    float cosR = std::cos(Particle.Rotation);
    float sinR = std::sin(Particle.Rotation);
    Math::Vec3 rotatedRight = CameraRight * cosR - CameraUp * sinR;
    Math::Vec3 rotatedUp = CameraRight * sinR + CameraUp * cosR;
    Math::Vec3 right = rotatedRight * HalfSize;
    Math::Vec3 up = rotatedUp * HalfSize;
    Math::Vec3 center = Particle.Position + (rotatedRight * OffsetX) + (rotatedUp * OffsetY);

    ParticleVertex Quad[4] = {
        {center.x - right.x - up.x, center.y - right.y - up.y, center.z - right.z - up.z, R, G, B, Alpha, 0.0f, 1.0f},
        {center.x + right.x - up.x, center.y + right.y - up.y, center.z + right.z - up.z, R, G, B, Alpha, 1.0f, 1.0f},
        {center.x - right.x + up.x, center.y - right.y + up.y, center.z - right.z + up.z, R, G, B, Alpha, 0.0f, 0.0f},
        {center.x + right.x + up.x, center.y + right.y + up.y, center.z + right.z + up.z, R, G, B, Alpha, 1.0f, 0.0f}
    };

    ParticleVertex Triangles[6] = {
        Quad[0], Quad[1], Quad[2],
        Quad[2], Quad[1], Quad[3]
    };

    size_t VertexSize = sizeof(ParticleVertex);
    for (int I = 0; I < 6; ++I) {
        if (VertexOffset + VertexSize <= Vertices.size()) {
            std::memcpy(Vertices.data() + VertexOffset, &Triangles[I], VertexSize);
            VertexOffset += VertexSize;
        }
    }
}

bgfx::VertexLayout UnifiedParticleSystemImpl::GetVertexLayout() const {
    bgfx::VertexLayout Layout;
    Layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    return Layout;
}

size_t UnifiedParticleSystemImpl::GetVertexSize() const {
    return sizeof(float) * 3 + sizeof(uint8_t) * 4 + sizeof(float) * 2; // 24 bytes
}

namespace ParticlePresets {

std::unique_ptr<ParticleSystem> CreateSnowParticleSystem(const SnowParticleConfig& Config) {
    return std::make_unique<SnowParticleSystemImpl>(Config);
}

std::unique_ptr<ParticleSystem> CreateFireParticleSystem(const FireParticleConfig& Config) {
    return std::make_unique<UnifiedParticleSystemImpl>(ParticlePresetType::Fire, Config);
}

std::unique_ptr<ParticleSystem> CreateElectricityParticleSystem(const ElectricityParticleConfig& Config) {
    return std::make_unique<UnifiedParticleSystemImpl>(ParticlePresetType::Electricity, Config);
}

std::unique_ptr<ParticleSystem> CreateSmokeParticleSystem(const SmokeParticleConfig& Config) {
    return std::make_unique<UnifiedParticleSystemImpl>(ParticlePresetType::Smoke, Config);
}

std::unique_ptr<ParticleSystem> CreateToxicGasParticleSystem(const ToxicGasParticleConfig& Config) {
    return std::make_unique<UnifiedParticleSystemImpl>(ParticlePresetType::ToxicGas, Config);
}

std::unique_ptr<ParticleSystem> CreateParticleSystem(ParticlePresetType type, const ParticlePresetConfig& config) {
    switch (type) {
        case ParticlePresetType::Snow:
            return std::make_unique<UnifiedParticleSystemImpl>(type, config);
        case ParticlePresetType::Fire:
            return std::make_unique<UnifiedParticleSystemImpl>(type, config);
        case ParticlePresetType::Electricity:
            return std::make_unique<UnifiedParticleSystemImpl>(type, config);
        case ParticlePresetType::Smoke:
            return std::make_unique<UnifiedParticleSystemImpl>(type, config);
        case ParticlePresetType::ToxicGas:
            return std::make_unique<UnifiedParticleSystemImpl>(type, config);
        default:
            return nullptr;
    }
}

} // namespace ParticlePresets

} // namespace Solstice::Render

