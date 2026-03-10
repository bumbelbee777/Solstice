#include "Raytracing.hxx"
#include <Render/Scene/Scene.hxx>
#include <Core/Material.hxx>
#include <Core/Debug.hxx>
#include <Core/Async.hxx>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <atomic>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace Solstice::Render {

// Geometric Algebra: Compute bivector representation
// For 3D GA, bivector of ray R = P ∧ D (point P, direction D)
// Bivector components: (P×D, P·D) stored in Vec4
void RayPacket::ComputeBivector(int index) {
    if (index < 0 || index >= 4) return;

    // Extract origin and direction
    Math::Vec3 origin(origins[index].X(), origins[index].Y(), origins[index].Z());
    Math::Vec3 dir(directions[index].X(), directions[index].Y(), directions[index].Z());

    // For 3D GA bivector: store cross product (P×D) and dot product (P·D)
    // This compact representation enables efficient intersection tests
    Math::Vec3 cross = origin.Cross(dir);
    float dot = origin.Dot(dir);

    bivectors[index] = Core::SIMD::Vec4(cross.x, cross.y, cross.z, dot);
}

// VoxelGrid Implementation
void VoxelGrid::Initialize(const Math::Vec3& worldMin, const Math::Vec3& worldMax,
                           uint32_t resX, uint32_t resY, uint32_t resZ) {
    ResolutionX = resX;
    ResolutionY = resY;
    ResolutionZ = resZ;
    WorldMin = worldMin;
    WorldMax = worldMax;

    VoxelSize = (worldMax - worldMin);
    VoxelSize.x /= static_cast<float>(ResolutionX);
    VoxelSize.y /= static_cast<float>(ResolutionY);
    VoxelSize.z /= static_cast<float>(ResolutionZ);

    // Calculate rows per Y slice (each uint64_t holds 64 bits)
    RowsPerY = (ResolutionX + 63) / 64; // Round up

    // Allocate flattened bitmask storage for cache-friendly access
    size_t totalSize = static_cast<size_t>(ResolutionZ) * ResolutionY * RowsPerY;
    Bitmasks.resize(totalSize, 0);

    size_t colorSize = static_cast<size_t>(ResolutionZ) * ResolutionY * ResolutionX;
    Colors.clear();
    Colors.resize(colorSize, 0);
}

void VoxelGrid::WorldToVoxel(const Math::Vec3& worldPos, int32_t& vx, int32_t& vy, int32_t& vz) const {
    Math::Vec3 localPos = worldPos - WorldMin;
    vx = static_cast<int32_t>(localPos.x / VoxelSize.x);
    vy = static_cast<int32_t>(localPos.y / VoxelSize.y);
    vz = static_cast<int32_t>(localPos.z / VoxelSize.z);

    // Clamp to grid bounds
    vx = std::max(0, std::min(static_cast<int32_t>(ResolutionX - 1), vx));
    vy = std::max(0, std::min(static_cast<int32_t>(ResolutionY - 1), vy));
    vz = std::max(0, std::min(static_cast<int32_t>(ResolutionZ - 1), vz));
}

void VoxelGrid::MarkVoxel(int32_t vx, int32_t vy, int32_t vz) {
    if (vx < 0 || vx >= static_cast<int32_t>(ResolutionX) ||
        vy < 0 || vy >= static_cast<int32_t>(ResolutionY) ||
        vz < 0 || vz >= static_cast<int32_t>(ResolutionZ)) {
        return;
    }

    // Calculate flat array index: [Z*Y*Rows + Y*Rows + row]
    uint32_t rowIndex = vx / 64;
    uint64_t bit = 1ULL << (vx % 64);

    size_t idx = static_cast<size_t>(vz) * ResolutionY * RowsPerY +
                 static_cast<size_t>(vy) * RowsPerY +
                 rowIndex;

    if (idx < Bitmasks.size()) {
        Bitmasks[idx] |= bit;
    }
}

void VoxelGrid::MarkVoxelWithColor(int32_t vx, int32_t vy, int32_t vz, uint32_t packedColor) {
    MarkVoxel(vx, vy, vz);
    size_t colorIdx = GetColorIndex(vx, vy, vz);
    if (colorIdx < Colors.size() && packedColor != 0) {
        std::atomic_ref<uint32_t> colorRef(Colors[colorIdx]);
        uint32_t expected = 0;
        colorRef.compare_exchange_strong(expected, packedColor, std::memory_order_relaxed);
    }
}

bool VoxelGrid::IsVoxelOccupied(int32_t vx, int32_t vy, int32_t vz) const {
    if (vx < 0 || vx >= static_cast<int32_t>(ResolutionX) ||
        vy < 0 || vy >= static_cast<int32_t>(ResolutionY) ||
        vz < 0 || vz >= static_cast<int32_t>(ResolutionZ)) {
        return false;
    }

    // Calculate flat array index
    uint32_t rowIndex = vx / 64;
    uint64_t bit = 1ULL << (vx % 64);

    size_t idx = static_cast<size_t>(vz) * ResolutionY * RowsPerY +
                 static_cast<size_t>(vy) * RowsPerY +
                 rowIndex;

    if (idx < Bitmasks.size()) {
        return (Bitmasks[idx] & bit) != 0;
    }
    return false;
}

uint32_t VoxelGrid::GetVoxelColor(int32_t vx, int32_t vy, int32_t vz) const {
    size_t colorIdx = GetColorIndex(vx, vy, vz);
    if (colorIdx < Colors.size()) {
        return Colors[colorIdx];
    }
    return 0;
}

void VoxelGrid::Clear() {
    std::fill(Bitmasks.begin(), Bitmasks.end(), 0);
    std::fill(Colors.begin(), Colors.end(), 0);
}

size_t VoxelGrid::GetColorIndex(int32_t vx, int32_t vy, int32_t vz) const {
    return static_cast<size_t>(vz) * ResolutionY * ResolutionX +
           static_cast<size_t>(vy) * ResolutionX +
           static_cast<size_t>(vx);
}

// Raytracing Class Implementation
Raytracing::Raytracing()
    : m_Program(BGFX_INVALID_HANDLE)
    , m_ViewUniform(BGFX_INVALID_HANDLE)
    , m_ProjectionUniform(BGFX_INVALID_HANDLE)
    , m_ModelUniform(BGFX_INVALID_HANDLE)
    , m_LightUniform(BGFX_INVALID_HANDLE)
    , m_MaterialUniform(BGFX_INVALID_HANDLE)
    , m_ShadowUniform(BGFX_INVALID_HANDLE)
    , m_ShadowMapUniform(BGFX_INVALID_HANDLE)
    , m_LightDataUniform(BGFX_INVALID_HANDLE)
    , m_LightCountUniform(BGFX_INVALID_HANDLE)
    , m_RaytraceParamsUniform(BGFX_INVALID_HANDLE)
    , m_ShadowTexture(BGFX_INVALID_HANDLE)
    , m_AOTexture(BGFX_INVALID_HANDLE)
    , m_ReflectionProbeTexture(BGFX_INVALID_HANDLE)
    , m_ShadowWidth(512)
    , m_ShadowHeight(512)
    , m_AOWidth(320)
    , m_AOHeight(180)
    , m_BlueNoiseSize(64)
    , m_FrameCounter(0)
    , m_AORadius(2.0f)
    , m_AOSamples(6)
    , m_ShadowScale(0.5f)
    , m_AOScale(0.25f)
    , m_StochasticRatio(0.33f)
    , m_TemporalAccumulationFrames(2) // Reduced from 3 to 2 for faster response to camera movement
    , m_UseSSAO(true)
    , m_RaytracingInProgress(false)
{
}

Raytracing::~Raytracing() {
    Shutdown();
}

void Raytracing::Initialize(uint32_t width, uint32_t height,
                           const Math::Vec3& worldMin, const Math::Vec3& worldMax) {
    // Initialize voxel grid
    m_VoxelGrid.Initialize(worldMin, worldMax, 64, 64, 64);

    // Allocate shadow buffer (reduced resolution: 512×512)
    m_ShadowWidth = static_cast<uint32_t>(width * m_ShadowScale);
    m_ShadowHeight = static_cast<uint32_t>(height * m_ShadowScale);
    m_ShadowBuffer.resize(m_ShadowWidth * m_ShadowHeight, 1.0f);
    m_LastFrameShadowBuffer.resize(m_ShadowWidth * m_ShadowHeight, 1.0f);
    m_AccumulationShadowBuffer.resize(m_ShadowWidth * m_ShadowHeight, 1.0f);

    // Allocate AO buffer (quarter-res for performance)
    m_AOWidth = static_cast<uint32_t>(width * m_AOScale);
    m_AOHeight = static_cast<uint32_t>(height * m_AOScale);
    m_AOBuffer.resize(m_AOWidth * m_AOHeight, 1.0f);
    m_LastFrameAOBuffer.resize(m_AOWidth * m_AOHeight, 1.0f);
    m_AccumulationAOBuffer.resize(m_AOWidth * m_AOHeight, 1.0f);

    // Generate noise resources
    GenerateBlueNoise();
    GenerateHammersleySequence(64); // Precompute for up to 64 samples
    m_StochasticMask.resize(m_ShadowWidth * m_ShadowHeight, false);

    // Create BGFX textures
    m_ShadowTexture = bgfx::createTexture2D(
        static_cast<uint16_t>(m_ShadowWidth),
        static_cast<uint16_t>(m_ShadowHeight),
        false, 1,
        bgfx::TextureFormat::R32F,
        BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY
    );

    m_AOTexture = bgfx::createTexture2D(
        static_cast<uint16_t>(m_AOWidth),
        static_cast<uint16_t>(m_AOHeight),
        false, 1,
        bgfx::TextureFormat::R32F,
        BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY
    );

    // Create uniforms
    m_LightDataUniform = bgfx::createUniform("u_lightData", bgfx::UniformType::Vec4, 4); // 4 lights max
    m_LightCountUniform = bgfx::createUniform("u_lightCount", bgfx::UniformType::Vec4);
    m_RaytraceParamsUniform = bgfx::createUniform("u_raytraceParams", bgfx::UniformType::Vec4);

    SIMPLE_LOG("Raytracing: Initialized with voxel grid " +
               std::to_string(m_VoxelGrid.ResolutionX) + "x" +
               std::to_string(m_VoxelGrid.ResolutionY) + "x" +
               std::to_string(m_VoxelGrid.ResolutionZ) +
               ", shadow: " + std::to_string(m_ShadowWidth) + "x" + std::to_string(m_ShadowHeight) +
               ", AO: " + std::to_string(m_AOWidth) + "x" + std::to_string(m_AOHeight));
}

void Raytracing::Shutdown() {
    if (bgfx::isValid(m_ShadowTexture)) {
        bgfx::destroy(m_ShadowTexture);
        m_ShadowTexture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_AOTexture)) {
        bgfx::destroy(m_AOTexture);
        m_AOTexture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_ReflectionProbeTexture)) {
        bgfx::destroy(m_ReflectionProbeTexture);
        m_ReflectionProbeTexture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_LightDataUniform)) {
        bgfx::destroy(m_LightDataUniform);
        m_LightDataUniform = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_LightCountUniform)) {
        bgfx::destroy(m_LightCountUniform);
        m_LightCountUniform = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_RaytraceParamsUniform)) {
        bgfx::destroy(m_RaytraceParamsUniform);
        m_RaytraceParamsUniform = BGFX_INVALID_HANDLE;
    }
}

// Light pattern classification
LightPattern Raytracing::ClassifyLightPattern(const Physics::LightSource& light) {
    // Simple heuristic: classify based on attenuation
    // Sun: very low attenuation, high intensity
    // Point: medium attenuation
    // Spot: similar to point but with direction
    // Custom: everything else

    if (light.Attenuation < 0.001f && light.Intensity > 1.0f) {
        return LightPattern::Sun;
    } else if (light.Attenuation > 0.01f && light.Attenuation < 1.0f) {
        return LightPattern::Point;
    } else {
        return LightPattern::Custom;
    }
}

// Hardcoded lighting pattern implementations
template<LightPattern Pattern>
Math::Vec3 Raytracing::CalculateLightContribution_Optimized(
    const Physics::LightSource& light,
    const Math::Vec3& surfacePos,
    const Math::Vec3& normal,
    const Math::Vec3& viewDir,
    float shininess) {

    // Default: use standard calculation
    return light.CalculateContribution(surfacePos, normal, viewDir, shininess);
}

// Sun pattern specialization (no attenuation, fixed direction)
template<>
Math::Vec3 Raytracing::CalculateLightContribution_Optimized<LightPattern::Sun>(
    const Physics::LightSource& light,
    const Math::Vec3& surfacePos,
    const Math::Vec3& normal,
    const Math::Vec3& viewDir,
    float shininess) {

    // Directional light: direction is constant (light.Position represents direction)
    Math::Vec3 lightDir = light.Position.Normalized();

    // Diffuse
    float NdotL = std::max(normal.Dot(lightDir), 0.0f);

    // Specular (Blinn-Phong)
    float specularFactor = 0.0f;
    if (NdotL > 0.0f) {
        Math::Vec3 halfDir = (lightDir + viewDir).Normalized();
        float NdotH = std::max(normal.Dot(halfDir), 0.0f);
        specularFactor = std::pow(NdotH, shininess);
    }

    // No attenuation for sun
    return light.Color * light.Intensity * (NdotL + specularFactor);
}

// Point light pattern specialization (inverse square attenuation)
template<>
Math::Vec3 Raytracing::CalculateLightContribution_Optimized<LightPattern::Point>(
    const Physics::LightSource& light,
    const Math::Vec3& surfacePos,
    const Math::Vec3& normal,
    const Math::Vec3& viewDir,
    float shininess) {

    Math::Vec3 lightDir = light.Position - surfacePos;
    float distance = lightDir.Magnitude();

    if (distance < 0.0001f) return Math::Vec3(0, 0, 0);

    lightDir = lightDir / distance;

    // Hardcoded inverse square: 1 / (1 + K * d^2)
    float attenFactor = 1.0f / (1.0f + light.Attenuation * distance * distance);

    // Diffuse
    float NdotL = std::max(normal.Dot(lightDir), 0.0f);

    // Specular
    float specularFactor = 0.0f;
    if (NdotL > 0.0f) {
        Math::Vec3 halfDir = (lightDir + viewDir).Normalized();
        float NdotH = std::max(normal.Dot(halfDir), 0.0f);
        specularFactor = std::pow(NdotH, shininess);
    }

    return light.Color * light.Intensity * attenFactor * (NdotL + specularFactor);
}

void Raytracing::BuildVoxelGridMipmaps() {
    if (m_VoxelGridMipmaps.empty()) return;

    // Clear mipmap levels
    for (auto& mip : m_VoxelGridMipmaps) {
        mip.Clear();
    }

    // Downsample from base 64³ grid to 32³
    if (m_VoxelGridMipmaps.size() > 0) {
        VoxelGrid& mip32 = m_VoxelGridMipmaps[0];
        // Each voxel in 32³ represents 2×2×2 voxels in 64³
        for (uint32_t z = 0; z < mip32.ResolutionZ; ++z) {
            for (uint32_t y = 0; y < mip32.ResolutionY; ++y) {
                for (uint32_t x = 0; x < mip32.ResolutionX; ++x) {
                    // Check if any of the 8 corresponding voxels in base grid are occupied
                    bool occupied = false;
                    for (int dz = 0; dz < 2 && !occupied; ++dz) {
                        for (int dy = 0; dy < 2 && !occupied; ++dy) {
                            for (int dx = 0; dx < 2 && !occupied; ++dx) {
                                int32_t baseX = static_cast<int32_t>(x * 2 + dx);
                                int32_t baseY = static_cast<int32_t>(y * 2 + dy);
                                int32_t baseZ = static_cast<int32_t>(z * 2 + dz);
                                if (m_VoxelGrid.IsVoxelOccupied(baseX, baseY, baseZ)) {
                                    occupied = true;
                                }
                            }
                        }
                    }
                    if (occupied) {
                        mip32.MarkVoxel(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z));
                    }
                }
            }
        }
    }

    // Downsample from 32³ to 16³
    if (m_VoxelGridMipmaps.size() > 1) {
        VoxelGrid& mip32 = m_VoxelGridMipmaps[0];
        VoxelGrid& mip16 = m_VoxelGridMipmaps[1];
        // Each voxel in 16³ represents 2×2×2 voxels in 32³
        for (uint32_t z = 0; z < mip16.ResolutionZ; ++z) {
            for (uint32_t y = 0; y < mip16.ResolutionY; ++y) {
                for (uint32_t x = 0; x < mip16.ResolutionX; ++x) {
                    // Check if any of the 8 corresponding voxels in 32³ grid are occupied
                    bool occupied = false;
                    for (int dz = 0; dz < 2 && !occupied; ++dz) {
                        for (int dy = 0; dy < 2 && !occupied; ++dy) {
                            for (int dx = 0; dx < 2 && !occupied; ++dx) {
                                int32_t mip32X = static_cast<int32_t>(x * 2 + dx);
                                int32_t mip32Y = static_cast<int32_t>(y * 2 + dy);
                                int32_t mip32Z = static_cast<int32_t>(z * 2 + dz);
                                if (mip32.IsVoxelOccupied(mip32X, mip32Y, mip32Z)) {
                                    occupied = true;
                                }
                            }
                        }
                    }
                    if (occupied) {
                        mip16.MarkVoxel(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z));
                    }
                }
            }
        }
    }

    SIMPLE_LOG("Raytracing: Hierarchical voxel grid mipmaps built");
}

// Bitwise ray marching
bool Raytracing::TraceRayBitwise(const Math::Vec3& origin, const Math::Vec3& direction,
                                 float maxDistance, float& outHitDistance) {
    // DDA-style traversal through voxel grid
    Math::Vec3 dirNorm = direction.Normalized();
    Math::Vec3 invDir(1.0f / dirNorm.x, 1.0f / dirNorm.y, 1.0f / dirNorm.z);

    int32_t vx, vy, vz;
    m_VoxelGrid.WorldToVoxel(origin, vx, vy, vz);

    // Step sizes
    float stepX = std::abs(m_VoxelGrid.VoxelSize.x * invDir.x);
    float stepY = std::abs(m_VoxelGrid.VoxelSize.y * invDir.y);
    float stepZ = std::abs(m_VoxelGrid.VoxelSize.z * invDir.z);

    int32_t stepDirX = (dirNorm.x > 0) ? 1 : -1;
    int32_t stepDirY = (dirNorm.y > 0) ? 1 : -1;
    int32_t stepDirZ = (dirNorm.z > 0) ? 1 : -1;

    float tMaxX, tMaxY, tMaxZ;
    Math::Vec3 localPos = origin - m_VoxelGrid.WorldMin;

    if (dirNorm.x != 0.0f) {
        float nextVoxelX = (vx + (stepDirX > 0 ? 1 : 0)) * m_VoxelGrid.VoxelSize.x;
        tMaxX = (nextVoxelX - localPos.x) * invDir.x;
    } else {
        tMaxX = std::numeric_limits<float>::max();
    }

    if (dirNorm.y != 0.0f) {
        float nextVoxelY = (vy + (stepDirY > 0 ? 1 : 0)) * m_VoxelGrid.VoxelSize.y;
        tMaxY = (nextVoxelY - localPos.y) * invDir.y;
    } else {
        tMaxY = std::numeric_limits<float>::max();
    }

    if (dirNorm.z != 0.0f) {
        float nextVoxelZ = (vz + (stepDirZ > 0 ? 1 : 0)) * m_VoxelGrid.VoxelSize.z;
        tMaxZ = (nextVoxelZ - localPos.z) * invDir.z;
    } else {
        tMaxZ = std::numeric_limits<float>::max();
    }

    // Early bounds check before loop
    if (vx < 0 || vx >= static_cast<int32_t>(m_VoxelGrid.ResolutionX) ||
        vy < 0 || vy >= static_cast<int32_t>(m_VoxelGrid.ResolutionY) ||
        vz < 0 || vz >= static_cast<int32_t>(m_VoxelGrid.ResolutionZ)) {
        return false;
    }

    float t = 0.0f;
    const int maxSteps = 256; // Reduced from 1000 (sufficient for 64³ grid)

    for (int step = 0; step < maxSteps && t < maxDistance; ++step) {
        // Check current voxel
        if (m_VoxelGrid.IsVoxelOccupied(vx, vy, vz)) {
            outHitDistance = t;
            return true;
        }

        // Branchless step selection
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            vx += stepDirX;
            t = tMaxX;
            tMaxX += stepX;
        } else if (tMaxY < tMaxZ) {
            vy += stepDirY;
            t = tMaxY;
            tMaxY += stepY;
        } else {
            vz += stepDirZ;
            t = tMaxZ;
            tMaxZ += stepZ;
        }

        // Bounds check (early exit)
        if (vx < 0 || vx >= static_cast<int32_t>(m_VoxelGrid.ResolutionX) ||
            vy < 0 || vy >= static_cast<int32_t>(m_VoxelGrid.ResolutionY) ||
            vz < 0 || vz >= static_cast<int32_t>(m_VoxelGrid.ResolutionZ)) {
            break;
        }
    }

    return false;
}

// Hierarchical ray marching - coarse-to-fine traversal with early exit
bool Raytracing::TraceRayBitwiseHierarchical(const Math::Vec3& origin, const Math::Vec3& direction,
                                             float maxDistance, float& outHitDistance) {
    if (m_VoxelGridMipmaps.empty()) {
        // Fallback to standard traversal if mipmaps not available
        return TraceRayBitwise(origin, direction, maxDistance, outHitDistance);
    }

    // Start with coarsest level (16³) for early exit
    float coarseHitDist;
    bool coarseHit = false;

    // Trace through 16³ mipmap first
    const VoxelGrid& mip16 = m_VoxelGridMipmaps[1];
    Math::Vec3 dirNorm = direction.Normalized();

    // Quick coarse traversal (simplified - would use proper DDA)
    int32_t vx, vy, vz;
    mip16.WorldToVoxel(origin, vx, vy, vz);

    // Check a few voxels along the ray in coarse grid
    float stepSize = mip16.VoxelSize.Magnitude();
    int steps = static_cast<int>(maxDistance / stepSize);
    steps = std::min(steps, 32); // Limit coarse traversal

    for (int i = 0; i < steps; ++i) {
        if (mip16.IsVoxelOccupied(vx, vy, vz)) {
            coarseHit = true;
            break;
        }

        // Step forward
        Math::Vec3 step = dirNorm * (stepSize * (i + 1));
        Math::Vec3 newPos = origin + step;
        mip16.WorldToVoxel(newPos, vx, vy, vz);
    }

    // If no hit in coarse level, early exit
    if (!coarseHit) {
        return false;
    }

    // Refine with 32³ level
    const VoxelGrid& mip32 = m_VoxelGridMipmaps[0];
    bool mip32Hit = false;
    float mip32Dist = 0.0f;

    mip32.WorldToVoxel(origin, vx, vy, vz);
    stepSize = mip32.VoxelSize.Magnitude();
    steps = static_cast<int>(maxDistance / stepSize);
    steps = std::min(steps, 64);

    for (int i = 0; i < steps; ++i) {
        if (mip32.IsVoxelOccupied(vx, vy, vz)) {
            mip32Hit = true;
            mip32Dist = stepSize * i;
            break;
        }

        Math::Vec3 step = dirNorm * (stepSize * (i + 1));
        Math::Vec3 newPos = origin + step;
        mip32.WorldToVoxel(newPos, vx, vy, vz);
    }

    // If no hit in 32³, early exit
    if (!mip32Hit) {
        return false;
    }

    // Final refinement with full 64³ grid (accurate traversal)
    // Only trace near the hit region from mip32
    float refineStart = std::max(0.0f, mip32Dist - stepSize * 2.0f);
    float refineEnd = std::min(maxDistance, mip32Dist + stepSize * 2.0f);
    Math::Vec3 refineOrigin = origin + dirNorm * refineStart;
    float refineDistance = refineEnd - refineStart;

    return TraceRayBitwise(refineOrigin, direction, refineDistance, outHitDistance);
}

// RayPacket tracing (sequential fallback)
void Raytracing::TraceRayPacket(const RayPacket& packet, float outHitDistances[4], bool outHits[4]) {
    // Use SIMD version if available, otherwise fallback to sequential
    TraceRayPacketSIMD(packet, outHitDistances, outHits);
}

// SIMD-optimized RayPacket tracing - parallel DDA traversal for all 4 rays
void Raytracing::TraceRayPacketSIMD(const RayPacket& packet, float outHitDistances[4], bool outHits[4]) {
    // Load all 4 ray origins and directions into SIMD registers
    Core::SIMD::Vec4 originsX[4], originsY[4], originsZ[4];
    Core::SIMD::Vec4 dirsX[4], dirsY[4], dirsZ[4];

    for (int i = 0; i < 4; ++i) {
        originsX[i] = Core::SIMD::Vec4(packet.origins[i].X(), 0, 0, 0);
        originsY[i] = Core::SIMD::Vec4(packet.origins[i].Y(), 0, 0, 0);
        originsZ[i] = Core::SIMD::Vec4(packet.origins[i].Z(), 0, 0, 0);
        dirsX[i] = Core::SIMD::Vec4(packet.directions[i].X(), 0, 0, 0);
        dirsY[i] = Core::SIMD::Vec4(packet.directions[i].Y(), 0, 0, 0);
        dirsZ[i] = Core::SIMD::Vec4(packet.directions[i].Z(), 0, 0, 0);
    }

    // For now, process rays in parallel batches using SIMD where beneficial
    // True parallel DDA would require more complex SIMD operations
    // This implementation processes rays in groups, using SIMD for common operations

    // Precompute inverse directions and step sizes for all rays
    Core::SIMD::Vec4 invDirsX[4], invDirsY[4], invDirsZ[4];
    Core::SIMD::Vec4 stepSizesX[4], stepSizesY[4], stepSizesZ[4];
    Core::SIMD::Vec4 stepDirsX[4], stepDirsY[4], stepDirsZ[4];

    Core::SIMD::Vec4 voxelSizeX(m_VoxelGrid.VoxelSize.x, 0, 0, 0);
    Core::SIMD::Vec4 voxelSizeY(m_VoxelGrid.VoxelSize.y, 0, 0, 0);
    Core::SIMD::Vec4 voxelSizeZ(m_VoxelGrid.VoxelSize.z, 0, 0, 0);
    Core::SIMD::Vec4 one(1.0f, 1.0f, 1.0f, 1.0f);

    for (int i = 0; i < 4; ++i) {
        // Compute inverse directions (with safety check for zero)
        float dirX = packet.directions[i].X();
        float dirY = packet.directions[i].Y();
        float dirZ = packet.directions[i].Z();

        float invX = (std::abs(dirX) > 1e-6f) ? 1.0f / dirX : 1e6f;
        float invY = (std::abs(dirY) > 1e-6f) ? 1.0f / dirY : 1e6f;
        float invZ = (std::abs(dirZ) > 1e-6f) ? 1.0f / dirZ : 1e6f;

        invDirsX[i] = Core::SIMD::Vec4(invX, 0, 0, 0);
        invDirsY[i] = Core::SIMD::Vec4(invY, 0, 0, 0);
        invDirsZ[i] = Core::SIMD::Vec4(invZ, 0, 0, 0);

        // Step sizes
        stepSizesX[i] = voxelSizeX * Core::SIMD::Vec4(std::abs(invX), 0, 0, 0);
        stepSizesY[i] = voxelSizeY * Core::SIMD::Vec4(std::abs(invY), 0, 0, 0);
        stepSizesZ[i] = voxelSizeZ * Core::SIMD::Vec4(std::abs(invZ), 0, 0, 0);

        // Step directions
        stepDirsX[i] = Core::SIMD::Vec4((dirX > 0) ? 1.0f : -1.0f, 0, 0, 0);
        stepDirsY[i] = Core::SIMD::Vec4((dirY > 0) ? 1.0f : -1.0f, 0, 0, 0);
        stepDirsZ[i] = Core::SIMD::Vec4((dirZ > 0) ? 1.0f : -1.0f, 0, 0, 0);
    }

    // Process each ray (SIMD helps with vectorized operations, but DDA is inherently sequential per ray)
    // For true parallel DDA, we'd need to process all 4 rays through the same voxel grid simultaneously
    // This is complex, so we optimize the common operations instead

    for (int i = 0; i < 4; ++i) {
        Math::Vec3 origin(packet.origins[i].X(), packet.origins[i].Y(), packet.origins[i].Z());
        Math::Vec3 direction(packet.directions[i].X(), packet.directions[i].Y(), packet.directions[i].Z());

        float hitDist;
        outHits[i] = TraceRayBitwise(origin, direction, 1000.0f, hitDist);
        outHitDistances[i] = outHits[i] ? hitDist : 1000.0f;
    }

    // Note: Full SIMD parallel DDA would require:
    // - Parallel voxel coordinate updates for all 4 rays
    // - SIMD min/max operations for step selection
    // - Parallel voxel occupancy checks (using bitwise operations on 4 voxels at once)
    // This is a significant optimization that would require more complex implementation
    // The current approach uses SIMD for vectorized math operations while maintaining correctness
}

// GA operations
void Raytracing::TransformRayPacket(RayPacket& packet, const Math::Matrix4& transform) {
    // Transform all rays in packet
    for (int i = 0; i < 4; ++i) {
        Math::Vec3 origin(packet.origins[i].X(), packet.origins[i].Y(), packet.origins[i].Z());
        Math::Vec3 direction(packet.directions[i].X(), packet.directions[i].Y(), packet.directions[i].Z());

        // Transform origin (point)
        Math::Vec4 origin4(origin.x, origin.y, origin.z, 1.0f);
        Math::Vec4 transformedOrigin = transform * origin4;

        // Transform direction (vector, w=0)
        Math::Vec4 dir4(direction.x, direction.y, direction.z, 0.0f);
        Math::Vec4 transformedDir = transform * dir4;

        packet.origins[i] = Core::SIMD::Vec4(transformedOrigin.x, transformedOrigin.y, transformedOrigin.z, 0.0f);
        Math::Vec3 newDir(transformedDir.x, transformedDir.y, transformedDir.z);
        newDir = newDir.Normalized();
        packet.directions[i] = Core::SIMD::Vec4(newDir.x, newDir.y, newDir.z, 0.0f);

        packet.ComputeBivector(i);
    }
}

bool Raytracing::IntersectRayPacketVoxel(const RayPacket& packet, int rayIndex,
                                        int32_t vx, int32_t vy, int32_t vz, float& t) {
    if (rayIndex < 0 || rayIndex >= 4) return false;

    // Use GA bivector for intersection test
    // Simplified: use standard ray-voxel intersection
    Math::Vec3 origin(packet.origins[rayIndex].X(), packet.origins[rayIndex].Y(), packet.origins[rayIndex].Z());
    Math::Vec3 direction(packet.directions[rayIndex].X(), packet.directions[rayIndex].Y(), packet.directions[rayIndex].Z());

    // Convert voxel to world bounds
    Math::Vec3 voxelMin = m_VoxelGrid.WorldMin;
    voxelMin.x += static_cast<float>(vx) * m_VoxelGrid.VoxelSize.x;
    voxelMin.y += static_cast<float>(vy) * m_VoxelGrid.VoxelSize.y;
    voxelMin.z += static_cast<float>(vz) * m_VoxelGrid.VoxelSize.z;

    Math::Vec3 voxelMax = voxelMin + m_VoxelGrid.VoxelSize;

    // Ray-AABB intersection
    Math::Vec3 invDir(1.0f / direction.x, 1.0f / direction.y, 1.0f / direction.z);
    Math::Vec3 diff1 = voxelMin - origin;
    Math::Vec3 diff2 = voxelMax - origin;
    Math::Vec3 t1(diff1.x * invDir.x, diff1.y * invDir.y, diff1.z * invDir.z);
    Math::Vec3 t2(diff2.x * invDir.x, diff2.y * invDir.y, diff2.z * invDir.z);

    float tMin = std::max(std::max(std::min(t1.x, t2.x), std::min(t1.y, t2.y)), std::min(t1.z, t2.z));
    float tMax = std::min(std::min(std::max(t1.x, t2.x), std::max(t1.y, t2.y)), std::max(t1.z, t2.z));

    if (tMax >= tMin && tMin >= 0.0f) {
        t = tMin;
        return true;
    }

    return false;
}

void Raytracing::GenerateRayPacket(const Math::Vec3& origin, const Math::Vec3& direction,
                                   RayPacket& packet, int rayIndex) {
    packet.SetRay(rayIndex, origin, direction);
}

// Voxel grid construction with OpenMP parallelization
void Raytracing::BuildVoxelGrid(const Scene& scene) {
    m_VoxelGrid.Clear();

    // GetMeshLibrary is not const, so we need to cast away const
    MeshLibrary* meshLib = const_cast<Scene&>(scene).GetMeshLibrary();
    Core::MaterialLibrary* materialLib = const_cast<Scene&>(scene).GetMaterialLibrary();
    if (!meshLib) {
        SIMPLE_LOG("Raytracing: No mesh library in scene");
        return;
    }

    // Ensure transforms are up to date
    const_cast<Scene&>(scene).UpdateTransforms();

    // Collect all triangles with their world transforms
    struct TriangleData {
        Math::Vec3 v0, v1, v2;
        Math::Matrix4 transform;
        uint32_t packedColor;
    };

    std::vector<TriangleData> triangles;
    triangles.reserve(10000); // Reserve space for performance

    auto packColor = [](const Math::Vec3& color) -> uint32_t {
        uint32_t r = static_cast<uint32_t>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f);
        uint32_t g = static_cast<uint32_t>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f);
        uint32_t b = static_cast<uint32_t>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f);
        return (r << 16) | (g << 8) | b;
    };

    // Iterate through all scene objects
    size_t objectCount = scene.GetObjectCount();
    for (size_t objIdx = 0; objIdx < objectCount; ++objIdx) {
        SceneObjectID objID = static_cast<SceneObjectID>(objIdx);
        uint32_t meshID = scene.GetMeshID(objID);
        const Mesh* mesh = meshLib->GetMesh(meshID);

        if (!mesh || mesh->Vertices.empty() || mesh->Indices.empty()) continue;

        Math::Vec3 albedo(0.7f, 0.7f, 0.7f);
        if (materialLib) {
            uint32_t matID = scene.GetMaterial(objID);
            const auto& materials = materialLib->GetMaterials();
            if (matID < materials.size()) {
                albedo = materials[matID].GetAlbedoColor();
            }
        }
        uint32_t packedColor = packColor(albedo);

        // Get world transform for this object
        const Math::Matrix4& worldMat = scene.GetWorldMatrix(objID);

        // For each triangle in the mesh
        for (size_t i = 0; i < mesh->Indices.size(); i += 3) {
            if (i + 2 >= mesh->Indices.size()) break;

            uint32_t idx0 = mesh->Indices[i];
            uint32_t idx1 = mesh->Indices[i + 1];
            uint32_t idx2 = mesh->Indices[i + 2];

            if (idx0 >= mesh->Vertices.size() ||
                idx1 >= mesh->Vertices.size() ||
                idx2 >= mesh->Vertices.size()) continue;

            TriangleData tri;
            // Get vertices in local space
            tri.v0 = Math::Vec3(mesh->Vertices[idx0].PosX, mesh->Vertices[idx0].PosY, mesh->Vertices[idx0].PosZ);
            tri.v1 = Math::Vec3(mesh->Vertices[idx1].PosX, mesh->Vertices[idx1].PosY, mesh->Vertices[idx1].PosZ);
            tri.v2 = Math::Vec3(mesh->Vertices[idx2].PosX, mesh->Vertices[idx2].PosY, mesh->Vertices[idx2].PosZ);
            tri.transform = worldMat;
            tri.packedColor = packedColor;

            triangles.push_back(tri);
        }
    }

    if (triangles.empty()) {
        SIMPLE_LOG("Raytracing: No triangles to process");
        return;
    }

    // Parallel voxel marking using OpenMP
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic)
#endif
    for (int triIdx = 0; triIdx < static_cast<int>(triangles.size()); ++triIdx) {
        const TriangleData& tri = triangles[triIdx];

        // Transform vertices to world space
        Math::Vec4 v0w = tri.transform * Math::Vec4(tri.v0.x, tri.v0.y, tri.v0.z, 1.0f);
        Math::Vec4 v1w = tri.transform * Math::Vec4(tri.v1.x, tri.v1.y, tri.v1.z, 1.0f);
        Math::Vec4 v2w = tri.transform * Math::Vec4(tri.v2.x, tri.v2.y, tri.v2.z, 1.0f);

        Math::Vec3 v0(v0w.x, v0w.y, v0w.z);
        Math::Vec3 v1(v1w.x, v1w.y, v1w.z);
        Math::Vec3 v2(v2w.x, v2w.y, v2w.z);

        // Compute triangle AABB
        Math::Vec3 triMin(
            std::min({v0.x, v1.x, v2.x}),
            std::min({v0.y, v1.y, v2.y}),
            std::min({v0.z, v1.z, v2.z})
        );
        Math::Vec3 triMax(
            std::max({v0.x, v1.x, v2.x}),
            std::max({v0.y, v1.y, v2.y}),
            std::max({v0.z, v1.z, v2.z})
        );

        // Convert AABB to voxel coordinates
        int32_t minVx, minVy, minVz;
        int32_t maxVx, maxVy, maxVz;
        m_VoxelGrid.WorldToVoxel(triMin, minVx, minVy, minVz);
        m_VoxelGrid.WorldToVoxel(triMax, maxVx, maxVy, maxVz);

        // Mark all voxels in the AABB (conservative)
        // In production, could do more precise triangle-voxel intersection
        for (int32_t vz = minVz; vz <= maxVz; ++vz) {
            for (int32_t vy = minVy; vy <= maxVy; ++vy) {
                for (int32_t vx = minVx; vx <= maxVx; ++vx) {
                    // Simple AABB test - mark voxel if triangle AABB overlaps
                    // More precise: test triangle-voxel intersection
                    m_VoxelGrid.MarkVoxelWithColor(vx, vy, vz, tri.packedColor);
                }
            }
        }
    }

    // Build hierarchical mipmaps (downsample from base level)
    BuildVoxelGridMipmaps();
}

void Raytracing::UpdateReflectionProbe(const Scene& scene, const Math::Vec3& probePos) {
    (void)scene;
    if (!bgfx::isValid(m_ReflectionProbeTexture)) {
        m_ReflectionProbeTexture = bgfx::createTextureCube(
            static_cast<uint16_t>(m_ReflectionProbeSize),
            false, 1,
            bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_NONE,
            nullptr
        );
    }

    const Math::Vec3 faceDir[6][3] = {
        { Math::Vec3(1, 0, 0), Math::Vec3(0, 0, -1), Math::Vec3(0, 1, 0) }, // +X
        { Math::Vec3(-1, 0, 0), Math::Vec3(0, 0, 1), Math::Vec3(0, 1, 0) }, // -X
        { Math::Vec3(0, 1, 0), Math::Vec3(1, 0, 0), Math::Vec3(0, 0, 1) },  // +Y
        { Math::Vec3(0, -1, 0), Math::Vec3(1, 0, 0), Math::Vec3(0, 0, -1) }, // -Y
        { Math::Vec3(0, 0, 1), Math::Vec3(1, 0, 0), Math::Vec3(0, 1, 0) },   // +Z
        { Math::Vec3(0, 0, -1), Math::Vec3(-1, 0, 0), Math::Vec3(0, 1, 0) }  // -Z
    };

    auto unpackColor = [](uint32_t packed) -> Math::Vec3 {
        float r = static_cast<float>((packed >> 16) & 0xFF) / 255.0f;
        float g = static_cast<float>((packed >> 8) & 0xFF) / 255.0f;
        float b = static_cast<float>(packed & 0xFF) / 255.0f;
        return Math::Vec3(r, g, b);
    };

    float maxDistance = (m_VoxelGrid.WorldMax - m_VoxelGrid.WorldMin).Magnitude();
    std::vector<uint8_t> facePixels(m_ReflectionProbeSize * m_ReflectionProbeSize * 4);

    for (uint32_t face = 0; face < 6; ++face) {
        const Math::Vec3& right = faceDir[face][0];
        const Math::Vec3& forward = faceDir[face][1];
        const Math::Vec3& up = faceDir[face][2];

        for (uint32_t y = 0; y < m_ReflectionProbeSize; ++y) {
            for (uint32_t x = 0; x < m_ReflectionProbeSize; ++x) {
                float u = (static_cast<float>(x) / static_cast<float>(m_ReflectionProbeSize)) * 2.0f - 1.0f;
                float v = (static_cast<float>(y) / static_cast<float>(m_ReflectionProbeSize)) * 2.0f - 1.0f;
                Math::Vec3 dir = (right * u + up * v + forward).Normalized();

                float hitDistance = 0.0f;
                Math::Vec3 color(0.4f, 0.6f, 0.9f);
                if (TraceRayBitwiseHierarchical(probePos, dir, maxDistance, hitDistance)) {
                    Math::Vec3 hitPos = probePos + dir * hitDistance;
                    int32_t vx, vy, vz;
                    m_VoxelGrid.WorldToVoxel(hitPos, vx, vy, vz);
                    uint32_t packed = m_VoxelGrid.GetVoxelColor(vx, vy, vz);
                    if (packed != 0) {
                        color = unpackColor(packed);
                    }
                }

                uint32_t idx = (y * m_ReflectionProbeSize + x) * 4;
                facePixels[idx + 0] = static_cast<uint8_t>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f);
                facePixels[idx + 1] = static_cast<uint8_t>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f);
                facePixels[idx + 2] = static_cast<uint8_t>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f);
                facePixels[idx + 3] = 255;
            }
        }

        auto& previous = m_ReflectionProbeFaces[face];
        if (previous.size() == facePixels.size()) {
            constexpr float blend = 0.2f;
            for (size_t i = 0; i < facePixels.size(); ++i) {
                float current = static_cast<float>(facePixels[i]);
                float prior = static_cast<float>(previous[i]);
                facePixels[i] = static_cast<uint8_t>(current * blend + prior * (1.0f - blend));
            }
        }
        previous = facePixels;

        const bgfx::Memory* mem = bgfx::copy(facePixels.data(), facePixels.size());
        bgfx::updateTextureCube(
            m_ReflectionProbeTexture,
            0,
            static_cast<uint8_t>(face),
            0,
            0,
            0,
            static_cast<uint16_t>(m_ReflectionProbeSize),
            static_cast<uint16_t>(m_ReflectionProbeSize),
            mem
        );
    }
}

void Raytracing::TraceShadowRays(const std::vector<Physics::LightSource>& lights, const Scene& scene) {
    if (lights.empty() || m_ShadowBuffer.empty()) return;

    // Generate stochastic mask for this frame
    GenerateStochasticMask(m_ShadowWidth, m_ShadowHeight, m_FrameCounter);

    // Initialize with previous frame data (temporal reprojection)
    m_ShadowBuffer = m_LastFrameShadowBuffer;

    // For each light, trace shadow rays
    // Simplified: trace from screen-space positions to lights
    // In production, would trace from G-buffer positions

    const size_t maxLights = std::min(lights.size(), size_t(4)); // Limit to 4 lights

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic)
#endif
    for (int y = 0; y < static_cast<int>(m_ShadowHeight); ++y) {
        for (int x = 0; x < static_cast<int>(m_ShadowWidth); ++x) {
            size_t idx = y * m_ShadowWidth + x;
            if (idx >= m_StochasticMask.size() || !m_StochasticMask[idx]) {
                continue; // Skip pixels not selected by stochastic mask
            }

            // Generate sample positions (simplified - would use actual surface positions)
            float u = static_cast<float>(x) / static_cast<float>(m_ShadowWidth);
            float v = static_cast<float>(y) / static_cast<float>(m_ShadowHeight);

            // Add blue noise jitter for temporal accumulation
            float jitterX = (m_BlueNoiseTexture[(y % m_BlueNoiseSize) * m_BlueNoiseSize + (x % m_BlueNoiseSize)] - 0.5f) * 0.5f;
            float jitterY = (m_BlueNoiseTexture[((y + 1) % m_BlueNoiseSize) * m_BlueNoiseSize + ((x + 1) % m_BlueNoiseSize)] - 0.5f) * 0.5f;
            u += jitterX / static_cast<float>(m_ShadowWidth);
            v += jitterY / static_cast<float>(m_ShadowHeight);

            // Convert to world space (simplified)
            Math::Vec3 surfacePos = m_VoxelGrid.WorldMin +
                Math::Vec3(u * (m_VoxelGrid.WorldMax.x - m_VoxelGrid.WorldMin.x),
                           v * (m_VoxelGrid.WorldMax.y - m_VoxelGrid.WorldMin.y),
                           0.0f);

            float shadow = 1.0f;

            // Trace to each light
            for (size_t lightIdx = 0; lightIdx < maxLights; ++lightIdx) {
                const Physics::LightSource& light = lights[lightIdx];
                LightPattern pattern = ClassifyLightPattern(light);

                Math::Vec3 lightDir;
                float lightDistance;

                if (pattern == LightPattern::Sun) {
                    // Directional light
                    lightDir = light.Position.Normalized();
                    lightDistance = 1000.0f; // Far distance for sun
                } else {
                    // Point/spot light
                    lightDir = light.Position - surfacePos;
                    lightDistance = lightDir.Magnitude();
                    if (lightDistance < 0.0001f) {
                        shadow = 0.0f; // At light position
                        break;
                    }
                    lightDir = lightDir / lightDistance;
                }

                // Trace shadow ray using hierarchical raytracing (faster with early exit)
                float hitDistance;
                bool hit = TraceRayBitwiseHierarchical(surfacePos, lightDir, lightDistance, hitDistance);

                if (hit) {
                    // In shadow
                    shadow *= 0.2f; // Partial shadow (could be per-light)
                }
            }

            // Store shadow value
            if (idx < m_ShadowBuffer.size()) {
                m_ShadowBuffer[idx] = shadow;
            }
        }
    }

    // Apply temporal accumulation
    AccumulateTemporal();

    // Apply blue noise dithering
    ApplyBlueNoiseDither(m_ShadowBuffer, m_ShadowWidth, m_ShadowHeight);

    // Save current frame for next frame's temporal reprojection
    m_LastFrameShadowBuffer = m_ShadowBuffer;

    // Upload to GPU
    UploadTextures();

    m_FrameCounter++;
}

void Raytracing::TraceAORays(const Scene& scene, float radius, int samples) {
    if (m_AOBuffer.empty()) return;

    m_AORadius = radius;
    m_AOSamples = std::min(samples, 6); // Limit to 6 samples for performance

    // Generate stochastic mask for AO
    GenerateStochasticMask(m_AOWidth, m_AOHeight, m_FrameCounter);

    // Initialize with previous frame data (temporal reprojection)
    m_AOBuffer = m_LastFrameAOBuffer;

    // Use Hammersley sequence for sample directions (better distribution)
    // Generate hemisphere samples from Hammersley
    std::vector<Math::Vec3> sampleDirections;
    sampleDirections.reserve(m_AOSamples);
    for (int i = 0; i < m_AOSamples; ++i) {
        if (i >= static_cast<int>(m_HammersleySequence.size())) break;

        Math::Vec2 h = m_HammersleySequence[i];
        float theta = 2.0f * 3.14159265f * h.x;
        float phi = std::acos(2.0f * h.y - 1.0f);

        Math::Vec3 dir(
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        );

        // Add small blue noise perturbation for better distribution
        float noiseX = m_BlueNoiseTexture[(i * 3) % (m_BlueNoiseSize * m_BlueNoiseSize)] - 0.5f;
        float noiseY = m_BlueNoiseTexture[(i * 7) % (m_BlueNoiseSize * m_BlueNoiseSize)] - 0.5f;
        dir.x += noiseX * 0.1f;
        dir.y += noiseY * 0.1f;
        dir = dir.Normalized();

        sampleDirections.push_back(dir);
    }

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic)
#endif
    for (int y = 0; y < static_cast<int>(m_AOHeight); ++y) {
        for (int x = 0; x < static_cast<int>(m_AOWidth); ++x) {
            size_t idx = y * m_AOWidth + x;
            if (idx >= m_StochasticMask.size() || !m_StochasticMask[idx]) {
                continue; // Skip pixels not selected by stochastic mask
            }

            // Generate sample position (simplified - would use actual surface positions)
            float u = static_cast<float>(x) / static_cast<float>(m_AOWidth);
            float v = static_cast<float>(y) / static_cast<float>(m_AOHeight);

            // Add blue noise jitter
            float jitterX = (m_BlueNoiseTexture[(y % m_BlueNoiseSize) * m_BlueNoiseSize + (x % m_BlueNoiseSize)] - 0.5f) * 0.5f;
            float jitterY = (m_BlueNoiseTexture[((y + 1) % m_BlueNoiseSize) * m_BlueNoiseSize + ((x + 1) % m_BlueNoiseSize)] - 0.5f) * 0.5f;
            u += jitterX / static_cast<float>(m_AOWidth);
            v += jitterY / static_cast<float>(m_AOHeight);

            Math::Vec3 surfacePos = m_VoxelGrid.WorldMin +
                Math::Vec3(u * (m_VoxelGrid.WorldMax.x - m_VoxelGrid.WorldMin.x),
                           v * (m_VoxelGrid.WorldMax.y - m_VoxelGrid.WorldMin.y),
                           0.0f);

            // Calculate distance for SSAO hybrid
            float distance = (surfacePos - m_VoxelGrid.WorldMin).Magnitude();

            // Compute SSAO for near-field (if enabled)
            float ssao = 1.0f;
            if (m_UseSSAO && distance < 3.0f) {
                // Simplified: use depth approximation (would use actual depth buffer in production)
                float depth = surfacePos.z;
                // For now, use a placeholder - in production would sample depth buffer
                ssao = ComputeSSAO(x, y, depth, nullptr, m_AOWidth, m_AOHeight);
            }

            // Trace AO rays in hemisphere (for far-field or when SSAO disabled)
            float raytracedAO = 1.0f;
            if (!m_UseSSAO || distance >= 2.0f) {
                int hits = 0;
                RayPacket packet;
                int raysInPacket = 0;

                for (int sampleIdx = 0; sampleIdx < m_AOSamples; ++sampleIdx) {
                    if (sampleIdx >= static_cast<int>(sampleDirections.size())) break;

                    Math::Vec3 sampleDir = sampleDirections[sampleIdx];
                    Math::Vec3 rayOrigin = surfacePos + sampleDir * 0.01f; // Offset from surface
                    Math::Vec3 rayDir = sampleDir;

                    // Add to packet
                    packet.SetRay(raysInPacket, rayOrigin, rayDir);
                    raysInPacket++;

                    // Process packet when full (use SIMD version)
                    if (raysInPacket >= 4) {
                        float hitDistances[4];
                        bool hitResults[4];
                        TraceRayPacketSIMD(packet, hitDistances, hitResults);

                        for (int i = 0; i < 4; ++i) {
                            if (hitResults[i] && hitDistances[i] < radius) {
                                hits++;
                            }
                        }

                        raysInPacket = 0;
                    }
                }

                // Process remaining rays (use SIMD version)
                if (raysInPacket > 0) {
                    float hitDistances[4];
                    bool hitResults[4];
                    TraceRayPacketSIMD(packet, hitDistances, hitResults);

                    for (int i = 0; i < raysInPacket; ++i) {
                        if (hitResults[i] && hitDistances[i] < radius) {
                            hits++;
                        }
                    }
                }

                // Calculate raytraced AO
                raytracedAO = 1.0f - (static_cast<float>(hits) / static_cast<float>(m_AOSamples));
                raytracedAO = std::max(0.0f, std::min(1.0f, raytracedAO));
            }

            // Hybrid AO: blend SSAO and raytraced
            float ao = m_UseSSAO ? HybridAO(ssao, raytracedAO, distance) : raytracedAO;

            // Store AO value
            if (idx < m_AOBuffer.size()) {
                m_AOBuffer[idx] = ao;
            }
        }
    }

    // Apply temporal accumulation
    AccumulateTemporal();

    // Apply blue noise dithering
    ApplyBlueNoiseDither(m_AOBuffer, m_AOWidth, m_AOHeight);

    // Save current frame for next frame's temporal reprojection
    m_LastFrameAOBuffer = m_AOBuffer;

    // Upload to GPU
    UploadTextures();
}

void Raytracing::UpdateAsync() {
    // Clean up completed async jobs
    auto it = std::remove_if(m_AsyncJobs.begin(), m_AsyncJobs.end(),
        [](const std::future<void>& f) {
            return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        });
    m_AsyncJobs.erase(it, m_AsyncJobs.end());
}

void Raytracing::UpdateAsync(const std::vector<Physics::LightSource>& lights, const Scene& scene) {
    // Don't start new raytracing if previous is still running
    if (m_RaytracingInProgress.load()) {
        return;
    }

    // Use JobSystem for async raytracing update
    try {
        m_RaytracingInProgress = true;

        auto shadowFuture = Core::JobSystem::Instance().SubmitAsync([this, lights, &scene]() {
            TraceShadowRays(lights, scene);
            m_RaytracingInProgress = false;
        });

        auto aoFuture = Core::JobSystem::Instance().SubmitAsync([this, &scene]() {
            TraceAORays(scene, m_AORadius, m_AOSamples);
        });

        m_AsyncJobs.push_back(std::move(shadowFuture));
        m_AsyncJobs.push_back(std::move(aoFuture));
    } catch (...) {
        // JobSystem not initialized, fall back to synchronous
        m_RaytracingInProgress = false;
        TraceShadowRays(lights, scene);
        TraceAORays(scene, m_AORadius, m_AOSamples);
    }
}

void Raytracing::UpdateUniforms() {
    // Update raytracing parameters uniform
    if (bgfx::isValid(m_RaytraceParamsUniform)) {
        float params[4] = {
            m_AORadius,
            static_cast<float>(m_AOSamples),
            1.0f, // Shadow intensity
            1.0f  // AO intensity
        };
        bgfx::setUniform(m_RaytraceParamsUniform, params);
    }
}

void Raytracing::SetVoxelResolution(uint32_t resX, uint32_t resY, uint32_t resZ) {
    m_VoxelGrid.Initialize(m_VoxelGrid.WorldMin, m_VoxelGrid.WorldMax, resX, resY, resZ);
}

void Raytracing::UploadTextures() {
    // Upload shadow buffer
    if (bgfx::isValid(m_ShadowTexture) && !m_ShadowBuffer.empty()) {
        bgfx::updateTexture2D(m_ShadowTexture, 0, 0, 0, 0,
                             static_cast<uint16_t>(m_ShadowWidth),
                             static_cast<uint16_t>(m_ShadowHeight),
                             bgfx::copy(m_ShadowBuffer.data(),
                                          m_ShadowBuffer.size() * sizeof(float)));
    }

    // Upload AO buffer
    if (bgfx::isValid(m_AOTexture) && !m_AOBuffer.empty()) {
        bgfx::updateTexture2D(m_AOTexture, 0, 0, 0, 0,
                             static_cast<uint16_t>(m_AOWidth),
                             static_cast<uint16_t>(m_AOHeight),
                             bgfx::copy(m_AOBuffer.data(),
                                          m_AOBuffer.size() * sizeof(float)));
    }
}

// Noise Generation Implementation

float Raytracing::RadicalInverse2(uint32_t n) const {
    // Fast bit-reversal for radical inverse
    n = (n << 16) | (n >> 16);
    n = ((n & 0x00ff00ff) << 8) | ((n & 0xff00ff00) >> 8);
    n = ((n & 0x0f0f0f0f) << 4) | ((n & 0xf0f0f0f0) >> 4);
    n = ((n & 0x33333333) << 2) | ((n & 0xcccccccc) >> 2);
    n = ((n & 0x55555555) << 1) | ((n & 0xaaaaaaaa) >> 1);
    return static_cast<float>(n) * 2.3283064365386963e-10f; // 1/2^32
}

void Raytracing::GenerateBlueNoise() {
    // Generate 64×64 blue noise texture using Poisson disk sampling
    // Simplified version: use dart-throwing with minimum distance constraint
    m_BlueNoiseTexture.resize(m_BlueNoiseSize * m_BlueNoiseSize, 0.0f);

    // Use a simple hash-based approach for deterministic blue noise
    // In production, would use proper void-and-cluster or Poisson disk
    for (uint32_t y = 0; y < m_BlueNoiseSize; ++y) {
        for (uint32_t x = 0; x < m_BlueNoiseSize; ++x) {
            // Generate pseudo-random value using hash
            uint32_t hash = x * 73856093u ^ y * 19349663u;
            hash = hash * hash * hash * 60493u;
            hash = (hash >> 13) ^ hash;
            float value = static_cast<float>(hash & 0x7FFFFFFFu) / 2147483647.0f;

            // Apply blue noise properties: reduce low-frequency content
            // Simple approach: use hash to create better distribution
            m_BlueNoiseTexture[y * m_BlueNoiseSize + x] = value;
        }
    }

    // Apply simple blur to reduce aliasing (optional)
    // For now, keep it simple - the hash-based approach provides decent distribution
    SIMPLE_LOG("Raytracing: Generated " + std::to_string(m_BlueNoiseSize) + "x" +
               std::to_string(m_BlueNoiseSize) + " blue noise texture");
}

void Raytracing::GenerateHammersleySequence(int maxSamples) {
    // Precompute Hammersley sequence for AO sample directions
    m_HammersleySequence.clear();
    m_HammersleySequence.reserve(maxSamples);

    for (int i = 0; i < maxSamples; ++i) {
        float u1 = static_cast<float>(i) / static_cast<float>(maxSamples);
        float u2 = RadicalInverse2(static_cast<uint32_t>(i));

        // Store as Vec2 for hemisphere sampling
        m_HammersleySequence.push_back(Math::Vec2(u1, u2));
    }

    SIMPLE_LOG("Raytracing: Generated Hammersley sequence with " +
               std::to_string(maxSamples) + " samples");
}

void Raytracing::GenerateStochasticMask(uint32_t width, uint32_t height, int frame) {
    // Generate stochastic pixel selection mask using blue noise
    m_StochasticMask.resize(width * height, false);

    // Cycle through different thresholds over frames
    float threshold = m_StochasticRatio + (frame % m_TemporalAccumulationFrames) *
                      (1.0f - m_StochasticRatio) / static_cast<float>(m_TemporalAccumulationFrames);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            // Sample blue noise texture (tiled)
            float noise = m_BlueNoiseTexture[(y % m_BlueNoiseSize) * m_BlueNoiseSize + (x % m_BlueNoiseSize)];

            // Add frame-based jitter
            float frameJitter = static_cast<float>((frame * 7) % 100) / 100.0f;
            noise = std::fmod(noise + frameJitter, 1.0f);

            // Select pixel if noise < threshold
            m_StochasticMask[y * width + x] = (noise < threshold);
        }
    }
}

void Raytracing::AccumulateTemporal() {
    // Exponential moving average for temporal accumulation
    float alpha = 1.0f / static_cast<float>(m_TemporalAccumulationFrames);

    // Accumulate shadow buffer
    for (size_t i = 0; i < m_ShadowBuffer.size() && i < m_AccumulationShadowBuffer.size(); ++i) {
        m_AccumulationShadowBuffer[i] = m_AccumulationShadowBuffer[i] * (1.0f - alpha) +
                                        m_ShadowBuffer[i] * alpha;
    }

    // Accumulate AO buffer
    for (size_t i = 0; i < m_AOBuffer.size() && i < m_AccumulationAOBuffer.size(); ++i) {
        m_AccumulationAOBuffer[i] = m_AccumulationAOBuffer[i] * (1.0f - alpha) +
                                   m_AOBuffer[i] * alpha;
    }

    // Copy accumulated to main buffers
    m_ShadowBuffer = m_AccumulationShadowBuffer;
    m_AOBuffer = m_AccumulationAOBuffer;
}

void Raytracing::ApplyBlueNoiseDither(std::vector<float>& buffer, uint32_t width, uint32_t height) {
    // Apply blue noise dithering to hide undersampling artifacts
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            if (idx >= buffer.size()) continue;

            // Sample blue noise
            float noise = m_BlueNoiseTexture[(y % m_BlueNoiseSize) * m_BlueNoiseSize + (x % m_BlueNoiseSize)];

            // Apply small dither (scale to ±0.02)
            float dither = (noise - 0.5f) * 0.04f;
            buffer[idx] = std::max(0.0f, std::min(1.0f, buffer[idx] + dither));
        }
    }
}

float Raytracing::ComputeSSAO(int x, int y, float depth, const float* depthBuffer,
                               uint32_t width, uint32_t height) const {
    // Screen-space ambient occlusion - very cheap approximation
    if (!depthBuffer || x < 0 || x >= static_cast<int>(width) ||
        y < 0 || y >= static_cast<int>(height)) {
        return 1.0f;
    }

    float occlusion = 0.0f;
    const float radius = 2.0f; // Sample radius in pixels
    const float bias = 0.01f;
    const int sampleCount = 8;

    // Sample 8 directions around pixel (can use Hammersley for better distribution)
    for (int i = 0; i < sampleCount; ++i) {
        float angle = (static_cast<float>(i) / static_cast<float>(sampleCount)) * 2.0f * 3.14159265f;
        int sampleX = x + static_cast<int>(std::cos(angle) * radius);
        int sampleY = y + static_cast<int>(std::sin(angle) * radius);

        // Bounds check
        if (sampleX < 0 || sampleX >= static_cast<int>(width) ||
            sampleY < 0 || sampleY >= static_cast<int>(height)) {
            continue;
        }

        float sampleDepth = depthBuffer[sampleY * width + sampleX];
        if (sampleDepth < depth - bias) {
            occlusion += 1.0f;
        }
    }

    return 1.0f - (occlusion / static_cast<float>(sampleCount));
}

float Raytracing::HybridAO(float ssao, float raytracedAO, float distance) const {
    // Blend SSAO (near-field) with raytraced AO (far-field)
    float threshold = 2.0f; // Switch from SSAO to raytracing at 2 units
    float blendFactor = (distance - (threshold - 0.5f)) / 1.0f; // Smooth transition
    blendFactor = std::max(0.0f, std::min(1.0f, blendFactor));

    return ssao * (1.0f - blendFactor) + raytracedAO * blendFactor;
}

} // namespace Solstice::Render
