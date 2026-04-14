#pragma once

#include <bgfx/bgfx.h>
#include <Math/Matrix.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <Physics/Lighting/LightSource.hxx>
#include <Core/ML/SIMD.hxx>
#include <vector>
#include <cstdint>
#include <memory>
#include <future>
#include <atomic>
#include <array>

// Forward declarations
namespace Solstice::Render { class Scene; }

namespace Solstice::Render {

// Hardcoded lighting patterns for optimization
enum class LightPattern {
    Sun,        // Directional - hardcoded: no attenuation, fixed direction
    Point,      // Point light - hardcoded: inverse square attenuation
    Spot,       // Spot light - hardcoded: cone angle, falloff
    Custom      // Use LightSource::CalculateContribution
};

// Geometric Algebra RayPacket - represents 4 rays for SIMD processing
// Uses 3D GA: ray as bivector R = origin ∧ direction
struct alignas(16) RayPacket {
    Core::SIMD::Vec4 origins[4];      // 4 ray origins (SIMD-aligned)
    Core::SIMD::Vec4 directions[4];   // 4 ray directions (normalized)
    Core::SIMD::Vec4 bivectors[4];    // GA bivector representation (compact)
    uint64_t bitmasks[4];             // Bitmask representation for voxel traversal

    RayPacket() {
        // Initialize to zero
        for (int i = 0; i < 4; ++i) {
            origins[i] = Core::SIMD::Vec4(0, 0, 0, 0);
            directions[i] = Core::SIMD::Vec4(0, 0, 0, 0);
            bivectors[i] = Core::SIMD::Vec4(0, 0, 0, 0);
            bitmasks[i] = 0;
        }
    }

    // Set a single ray in the packet (index 0-3)
    void SetRay(int index, const Math::Vec3& origin, const Math::Vec3& direction) {
        if (index < 0 || index >= 4) return;
        origins[index] = Core::SIMD::Vec4(origin.x, origin.y, origin.z, 0.0f);
        Math::Vec3 dirNorm = direction.Normalized();
        directions[index] = Core::SIMD::Vec4(dirNorm.x, dirNorm.y, dirNorm.z, 0.0f);
        // Compute bivector: origin ∧ direction (wedge product)
        // For 3D GA, bivector components stored in Vec4
        ComputeBivector(index);
    }

    // Make ComputeBivector accessible to Raytracing class
    friend class Raytracing;
private:
    void ComputeBivector(int index);
};

// Voxel Grid - stores occupancy as bitmasks (optimized: flat array for cache efficiency)
struct VoxelGrid {
    uint32_t ResolutionX, ResolutionY, ResolutionZ;
    Math::Vec3 WorldMin, WorldMax;
    Math::Vec3 VoxelSize;

    // Flattened bitmask storage for cache-friendly access
    // Layout: [Z * ResolutionY * RowsPerY + Y * RowsPerY + RowX]
    // Each row (X direction) is a uint64_t, supporting up to 64 voxels per row
    std::vector<uint64_t> Bitmasks; // Flat contiguous array
    uint32_t RowsPerY; // Number of uint64_t rows per Y slice
    std::vector<uint32_t> Colors; // Packed RGB (0xRRGGBB), 0 = unset

    VoxelGrid() : ResolutionX(64), ResolutionY(64), ResolutionZ(64), RowsPerY(1) {}

    void Initialize(const Math::Vec3& worldMin, const Math::Vec3& worldMax,
                    uint32_t resX = 64, uint32_t resY = 64, uint32_t resZ = 64);

    // Convert world position to voxel coordinates
    void WorldToVoxel(const Math::Vec3& worldPos, int32_t& vx, int32_t& vy, int32_t& vz) const;

    // Mark voxel as occupied
    void MarkVoxel(int32_t vx, int32_t vy, int32_t vz);
    void MarkVoxelWithColor(int32_t vx, int32_t vy, int32_t vz, uint32_t packedColor);

    // Check if voxel is occupied (optimized with flat array access)
    bool IsVoxelOccupied(int32_t vx, int32_t vy, int32_t vz) const;
    uint32_t GetVoxelColor(int32_t vx, int32_t vy, int32_t vz) const;

    // Clear all voxels
    void Clear();
    size_t GetColorIndex(int32_t vx, int32_t vy, int32_t vz) const;
};

class SOLSTICE_API Raytracing {
public:
    Raytracing();
    ~Raytracing();

    // Initialization
    void Initialize(uint32_t width, uint32_t height,
                   const Math::Vec3& worldMin, const Math::Vec3& worldMax);
    void Shutdown();

    // Voxel grid construction (OpenMP parallelized)
    void BuildVoxelGrid(const Scene& scene);
    void BuildVoxelGridMipmaps();

    // Ray tracing with GA RayPackets
    void TraceShadowRays(const std::vector<Physics::LightSource>& lights, const Scene& scene);
    void TraceAORays(const Scene& scene, float radius = 2.0f, int samples = 16);
    void TraceRayPacket(const RayPacket& packet, float outHitDistances[4], bool outHits[4]);
    void TraceRayPacketSIMD(const RayPacket& packet, float outHitDistances[4], bool outHits[4]);

    // Async update
    void UpdateAsync();
    void UpdateAsync(const std::vector<Physics::LightSource>& lights, const Scene& scene);

    // Texture access
    bgfx::TextureHandle GetShadowTexture() const { return m_ShadowTexture; }
    bgfx::TextureHandle GetAOTexture() const { return m_AOTexture; }

    // Uniform updates
    void UpdateUniforms();

    // Settings
    void SetVoxelResolution(uint32_t resX, uint32_t resY, uint32_t resZ);
    void SetAORadius(float radius) { m_AORadius = radius; }
    void SetAOSamples(int samples) { m_AOSamples = std::min(samples, 6); }
    void SetShadowScale(float scale) { m_ShadowScale = std::max(0.1f, std::min(1.0f, scale)); }
    void SetAOScale(float scale) { m_AOScale = std::max(0.1f, std::min(1.0f, scale)); }
    void SetStochasticRatio(float ratio) { m_StochasticRatio = std::max(0.1f, std::min(1.0f, ratio)); }
    void SetTemporalAccumulationFrames(int frames) { m_TemporalAccumulationFrames = std::max(1, std::min(8, frames)); }
    void SetUseSSAO(bool use) { m_UseSSAO = use; }
    void UpdateReflectionProbe(const Scene& scene, const Math::Vec3& probePos);
    bgfx::TextureHandle GetReflectionProbeTexture() const { return m_ReflectionProbeTexture; }

    // Light pattern matching
    static LightPattern ClassifyLightPattern(const Physics::LightSource& light);

private:
    // BGFX resources
    bgfx::ProgramHandle m_Program;
    bgfx::UniformHandle m_ViewUniform;
    bgfx::UniformHandle m_ProjectionUniform;
    bgfx::UniformHandle m_ModelUniform;
    bgfx::UniformHandle m_LightUniform;
    bgfx::UniformHandle m_MaterialUniform;
    bgfx::UniformHandle m_ShadowUniform;
    bgfx::UniformHandle m_ShadowMapUniform;
    bgfx::UniformHandle m_LightDataUniform;
    bgfx::UniformHandle m_LightCountUniform;
    bgfx::UniformHandle m_RaytraceParamsUniform;

    // Textures
    bgfx::TextureHandle m_ShadowTexture;
    bgfx::TextureHandle m_AOTexture;
    bgfx::TextureHandle m_ReflectionProbeTexture;

    // Voxel grid
    VoxelGrid m_VoxelGrid;
    std::vector<VoxelGrid> m_VoxelGridMipmaps; // Hierarchical mipmap levels (64³→32³→16³)

    // Shadow/AO buffers (CPU-side)
    std::vector<float> m_ShadowBuffer;
    std::vector<float> m_AOBuffer;
    uint32_t m_ShadowWidth, m_ShadowHeight;
    uint32_t m_AOWidth, m_AOHeight;

    // Temporal buffers for accumulation
    std::vector<float> m_LastFrameShadowBuffer;
    std::vector<float> m_LastFrameAOBuffer;
    std::vector<float> m_AccumulationShadowBuffer;
    std::vector<float> m_AccumulationAOBuffer;

    // Noise resources
    std::vector<float> m_BlueNoiseTexture; // 64×64 blue noise texture
    std::vector<Math::Vec2> m_HammersleySequence; // Precomputed Hammersley sequence for AO
    std::vector<bool> m_StochasticMask; // Per-pixel selection mask
    uint32_t m_BlueNoiseSize;
    uint32_t m_FrameCounter;

    // Async job futures
    std::vector<std::future<void>> m_AsyncJobs;
    std::atomic<bool> m_RaytracingInProgress;

    // Settings
    float m_AORadius;
    int m_AOSamples;
    float m_ShadowScale; // Resolution scale (default 0.5 for 512×512)
    float m_AOScale; // Resolution scale (default 0.25 for quarter-res)
    float m_StochasticRatio; // Percentage of pixels to trace per frame (0.25-0.5)
    int m_TemporalAccumulationFrames; // Number of frames to accumulate (2-4)
    bool m_UseSSAO; // Enable SSAO hybrid

    uint32_t m_ReflectionProbeSize = 16;
    std::array<std::vector<uint8_t>, 6> m_ReflectionProbeFaces;

    // Internal methods
    void UploadTextures();
    void GenerateRayPacket(const Math::Vec3& origin, const Math::Vec3& direction,
                           RayPacket& packet, int rayIndex);

    // Noise generation
    void GenerateBlueNoise();
    void GenerateHammersleySequence(int maxSamples = 64);
    void GenerateStochasticMask(uint32_t width, uint32_t height, int frame);
    float RadicalInverse2(uint32_t n) const;

    // Temporal accumulation
    void AccumulateTemporal();
    void ApplyBlueNoiseDither(std::vector<float>& buffer, uint32_t width, uint32_t height);

    // SSAO approximation
    float ComputeSSAO(int x, int y, float depth, const float* depthBuffer, uint32_t width, uint32_t height) const;
    float HybridAO(float ssao, float raytracedAO, float distance) const;

    // Hardcoded lighting pattern implementations
    template<LightPattern Pattern>
    Math::Vec3 CalculateLightContribution_Optimized(
        const Physics::LightSource& light,
        const Math::Vec3& surfacePos,
        const Math::Vec3& normal,
        const Math::Vec3& viewDir,
        float shininess);

    // Bitwise ray marching
    bool TraceRayBitwise(const Math::Vec3& origin, const Math::Vec3& direction,
                        float maxDistance, float& outHitDistance);
    bool TraceRayBitwiseHierarchical(const Math::Vec3& origin, const Math::Vec3& direction,
                                    float maxDistance, float& outHitDistance);

    // GA operations
    void TransformRayPacket(RayPacket& packet, const Math::Matrix4& transform);
    bool IntersectRayPacketVoxel(const RayPacket& packet, int rayIndex,
                                 int32_t vx, int32_t vy, int32_t vz, float& t);
};

} // namespace Solstice::Render
