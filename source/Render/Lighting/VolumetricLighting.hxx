#pragma once

#include <Solstice.hxx>
#include <Core/ML/SIMD.hxx>
#include <Core/System/Async.hxx>
#include <Physics/Lighting/LightSource.hxx>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <bgfx/bgfx.h>
#include <vector>
#include <array>
#include <cstdint>
#include <future>
#include <atomic>
#include <unordered_map>

namespace Solstice::Render {

// Forward declarations
class Scene;

// ============================================================================
// Processing Tiers - Different code paths based on light count
// ============================================================================
enum class ProcessingTier {
    Fast,       // 1-8 lights: full quality, per-light processing
    Batched,    // 9-32 lights: SIMD 4-light batches
    Clustered   // 33+ lights: cluster similar lights, process representatives
};

// ============================================================================
// Light Cluster - Groups nearby lights for efficient processing
// ============================================================================
struct LightCluster {
    Math::Vec3 CentroidPosition;  // Weighted center of cluster
    Math::Vec3 AverageColor;      // Average color of lights in cluster
    float TotalIntensity = 0.0f;  // Sum of intensities
    float EffectiveRange = 0.0f;  // Max range in cluster
    int LightCount = 0;           // Number of lights represented

    LightCluster() : CentroidPosition(0, 0, 0), AverageColor(0, 0, 0) {}
};

// ============================================================================
// Bitplane storage for light occlusion
// ============================================================================
struct SOLSTICE_API LightOcclusionGrid {
    uint32_t ResolutionX = 64;
    uint32_t ResolutionY = 64;
    uint32_t ResolutionZ = 64;
    uint32_t RowsPerY = 1;
    std::vector<uint64_t> Bitmasks; // Flat cache-friendly storage
    Math::Vec3 WorldMin;
    Math::Vec3 WorldMax;
    Math::Vec3 VoxelSize;

    // Precomputed reciprocals (avoid division in hot paths)
    float VoxelSizeInvX = 1.0f;
    float VoxelSizeInvY = 1.0f;
    float VoxelSizeInvZ = 1.0f;

    void Initialize(const Math::Vec3& InWorldMin, const Math::Vec3& InWorldMax,
                   uint32_t ResX = 64, uint32_t ResY = 64, uint32_t ResZ = 64);
    void WorldToVoxel(const Math::Vec3& WorldPos, int32_t& OutVX, int32_t& OutVY, int32_t& OutVZ) const;
    void MarkOccluder(int32_t VX, int32_t VY, int32_t VZ);
    bool IsOccluded(int32_t VX, int32_t VY, int32_t VZ) const;
    void Clear();
};

// ============================================================================
// Light contribution packet - SIMD optimized for 4 lights
// ============================================================================
struct alignas(16) LightPacket {
    Core::SIMD::Vec4 Positions[4];    // XYZ + Range
    Core::SIMD::Vec4 Colors[4];       // RGB + Intensity
    Core::SIMD::Vec4 Attenuations;    // 4 attenuation values
    int32_t LightCount = 0;

    // Calculate contribution for 4 lights simultaneously
    void CalculateContribution4(
        const Math::Vec3& SurfacePos,
        const Math::Vec3& Normal,
        float OutContributions[4]) const;

    // Calculate volumetric contribution (isotropic scattering, no normal dependency)
    void CalculateVolumetricContribution4(
        const Math::Vec3& SamplePos,
        float OutContributions[4]) const;

    // SIMD-optimized volumetric contribution using precomputed falloff LUT
    void CalculateVolumetricContribution4_Fast(
        const Math::Vec3& SamplePos,
        const float* FalloffLUT,
        int LUTSize,
        float LUTMaxDist,
        float OutContributions[4]) const;

    // Add a light to the packet
    void AddLight(const Physics::LightSource& Light);

    // Add a cluster as a proxy light
    void AddCluster(const LightCluster& Cluster);

    // Reset packet
    void Reset() { LightCount = 0; }
};

// ============================================================================
// Volumetric ray march result
// ============================================================================
struct VolumetricSample {
    float Density = 0.0f;     // 0-1 accumulated light density
    Math::Vec3 Color;         // Accumulated color

    VolumetricSample() : Color(0.0f, 0.0f, 0.0f) {}
};

// ============================================================================
// Async buffer for double-buffered processing
// ============================================================================
struct AsyncVolumetricBuffer {
    std::vector<float> R, G, B;
    std::atomic<bool> Ready{false};

    void Resize(size_t Size) {
        R.resize(Size, 0.0f);
        G.resize(Size, 0.0f);
        B.resize(Size, 0.0f);
    }

    void Clear() {
        std::fill(R.begin(), R.end(), 0.0f);
        std::fill(G.begin(), G.end(), 0.0f);
        std::fill(B.begin(), B.end(), 0.0f);
    }
};

// ============================================================================
// Volumetric lighting system with full optimizations
// ============================================================================
class SOLSTICE_API VolumetricLighting {
public:
    VolumetricLighting();
    ~VolumetricLighting();

    void Initialize(uint32_t Width, uint32_t Height,
                   const Math::Vec3& WorldMin, const Math::Vec3& WorldMax);
    void Shutdown();

    // Build occlusion grid from scene geometry (call when scene changes)
    void BuildOcclusionGrid(const Scene& SceneGraph);
    void BuildOcclusionMipmaps();
    void MarkOcclusionDirty() { m_OcclusionGridDirty = true; }

    // Trace volumetric rays - main entry point (uses async internally)
    void TraceVolumetricRays(
        const std::vector<Physics::LightSource>& Lights,
        const Math::Vec3& CameraPos,
        const Math::Matrix4& ViewProj);

    // Get output texture for compositing
    bgfx::TextureHandle GetVolumetricTexture() const { return m_VolumetricTexture; }

    // Settings
    void SetExposure(float Exposure) { m_Exposure = Exposure; }
    void SetNumRaysPerLight(int Rays) { m_NumRaysPerLight = std::max(2, std::min(Rays, 8)); }
    void SetRaySpreadAngle(float Angle) { m_RaySpreadAngle = std::max(0.1f, std::min(Angle, 3.14159f)); }
    void SetSimpleFalloff(float Falloff) { m_SimpleFalloff = std::max(0.01f, Falloff); RebuildFalloffLUT(); }
    void SetStochasticRatio(float Ratio) { m_StochasticRatio = Ratio; }
    void SetDensity(float Density) { m_Density = std::max(0.0f, Density); }

    float GetExposure() const { return m_Exposure; }
    int GetNumRaysPerLight() const { return m_NumRaysPerLight; }
    float GetRaySpreadAngle() const { return m_RaySpreadAngle; }
    float GetSimpleFalloff() const { return m_SimpleFalloff; }
    float GetDensity() const { return m_Density; }

    // Light count limits
    static constexpr int MAX_VOLUMETRIC_LIGHTS = 24;
    static constexpr int MAX_CLUSTERS = 12;

private:
    // ========== Occlusion Grid ==========
    LightOcclusionGrid m_OcclusionGrid;
    std::vector<LightOcclusionGrid> m_OcclusionMipmaps;
    bool m_OcclusionGridDirty = true;

    // ========== Double-Buffered Output ==========
    std::array<AsyncVolumetricBuffer, 2> m_AsyncBuffers;
    int m_CurrentReadBuffer = 0;
    std::vector<float> m_PrevBufferR;
    std::vector<float> m_PrevBufferG;
    std::vector<float> m_PrevBufferB;
    std::vector<float> m_VolumetricBufferR;
    std::vector<float> m_VolumetricBufferG;
    std::vector<float> m_VolumetricBufferB;
    std::vector<bool> m_StochasticMask;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    // ========== Async State ==========
    std::vector<std::future<void>> m_PendingTileFutures;
    Core::ExecutionGuard m_TraceGuard;
    std::atomic<bool> m_AsyncProcessingActive{false};
    std::future<void> m_PrecomputeFuture;
    std::atomic<bool> m_PrecomputeReady{false};

    // ========== Precomputed LUTs ==========
    static constexpr int FALLOFF_LUT_SIZE = 256;
    static constexpr float FALLOFF_LUT_MAX_DIST = 50.0f;
    std::array<float, FALLOFF_LUT_SIZE> m_FalloffLUT;
    std::array<float, FALLOFF_LUT_SIZE> m_RangeFalloffLUT;

    // ========== Precomputed Ray Directions ==========
    static constexpr int MAX_PRECOMPUTED_RAYS = 16;
    std::array<Math::Vec2, MAX_PRECOMPUTED_RAYS> m_PrecomputedRayDirs;

    // ========== Precomputed Screen Positions ==========
    std::vector<Math::Vec3> m_ScreenPosCache;
    Math::Matrix4 m_CachedInvViewProj;

    // ========== Precomputed Stochastic Masks ==========
    static constexpr int NUM_STOCHASTIC_MASKS = 4;
    std::array<std::vector<bool>, NUM_STOCHASTIC_MASKS> m_PrecomputedMasks;

    // ========== Precomputed Half Conversion ==========
    std::array<uint16_t, 256> m_FloatToHalfLUT;

    // ========== Blue Noise ==========
    std::vector<float> m_BlueNoiseTexture;
    uint32_t m_BlueNoiseSize = 64;
    uint32_t m_FrameCounter = 0;

    // ========== BGFX Resources ==========
    bgfx::TextureHandle m_VolumetricTexture = BGFX_INVALID_HANDLE;

    // ========== Settings (Optimized Defaults) ==========
    float m_Exposure = 1.0f;
    int m_NumRaysPerLight = 8;         // Increased for better coverage
    float m_RaySpreadAngle = 1.57f;    // 90 degrees spread
    float m_SimpleFalloff = 0.1f;
    float m_StochasticRatio = 0.75f;    // Increased for more visible rays
    float m_Density = 1.0f;
    int m_TemporalFrames = 2;          // Reduced from 4
    int m_RayMarchSteps = 8;           // Increased for better quality

    // ========== Tiled Processing ==========
    static constexpr int TILE_SIZE = 32;
    static constexpr int TILES_PER_JOB = 4;

    // ========== Precomputation Methods ==========
    void PrecomputeAll();
    void PrecomputeFalloffLUT();
    void RebuildFalloffLUT();
    void PrecomputeRayDirections();
    void PrecomputeStochasticMasks();
    void PrecomputeHalfConversion();
    void GenerateBlueNoise();

    // ========== Screen Position Cache ==========
    void UpdateScreenPosCache(const Math::Matrix4& InvViewProj);
    const Math::Vec3& GetCachedWorldPos(int X, int Y) const;

    // ========== Light Clustering ==========
    ProcessingTier DetermineProcessingTier(size_t LightCount) const;
    void ClusterLights(const std::vector<Physics::LightSource>& Lights,
                       std::vector<LightCluster>& OutClusters,
                       int MaxClusters = MAX_CLUSTERS);
    static int64_t HashGridCell(const Math::Vec3& Pos, float CellSize);

    // ========== Tiered Processing ==========
    void ProcessLights_Fast(const std::vector<Physics::LightSource>& Lights,
                            const Math::Vec3& CameraPos,
                            const Math::Matrix4& InvViewProj,
                            int WriteBuffer);
    void ProcessLights_Batched(const std::vector<Physics::LightSource>& Lights,
                               const Math::Vec3& CameraPos,
                               const Math::Matrix4& InvViewProj,
                               int WriteBuffer);
    void ProcessLights_Clustered(const std::vector<LightCluster>& Clusters,
                                 const Math::Vec3& CameraPos,
                                 const Math::Matrix4& InvViewProj,
                                 int WriteBuffer);

    // ========== Tile Processing ==========
    void ProcessTile_Fast(int TileX, int TileY,
                          const std::vector<LightPacket>& Packets,
                          int WriteBuffer);
    void ProcessTile_Clustered(int TileX, int TileY,
                               const std::vector<LightCluster>& Clusters,
                               int WriteBuffer);
    void ProcessTile_Async(int TileX, int TileY, int WriteBuffer,
                          const std::vector<Physics::LightSource>& Lights,
                          const std::vector<LightCluster>& Clusters,
                          ProcessingTier Tier,
                          const Math::Vec3& CameraPos,
                          const Math::Matrix4& InvViewProj);

    // ========== Temporal & Upload ==========
    void AccumulateTemporal(int WriteBuffer);
    void AccumulateTemporal_Async(int WriteBuffer);
    void AccumulateTemporal();  // Legacy version without WriteBuffer
    void UploadTexture();

    // ========== Validation Helpers ==========
    std::vector<Physics::LightSource> ValidateLights(
        const std::vector<Physics::LightSource>& Lights) const;
    bool ValidateLightingData() const;

    // ========== Legacy Methods (simplified) ==========
    void PackLightsIntoPackets(const std::vector<Physics::LightSource>& Lights,
                               std::vector<LightPacket>& OutPackets);
    float LookupFalloff(float Distance) const;
    uint16_t FastFloatToHalf(float Value) const;
    float CalculateSimpleContribution(const Math::Vec3& SamplePos,
                                     const Physics::LightSource& Light) const;
    bool GenerateRadialRays(const Physics::LightSource& Light,
                           const Math::Matrix4& ViewProj,
                           Math::Vec2& OutLightScreenPos,
                           std::vector<Math::Vec2>& OutRayDirs2D);
    Math::Vec3 MarchScreenSpaceRay(int PixelX, int PixelY,
                                  const Math::Vec2& RayDir2D,
                                  const Math::Vec2& LightScreenPos,
                                  const Physics::LightSource& Light,
                                  const Math::Matrix4& InvViewProj);
    void ProcessTile(int TileX, int TileY,
                     const std::vector<Physics::LightSource>& Lights,
                     const Math::Vec3& CameraPos,
                     const Math::Matrix4& InvViewProj);
    void GenerateStochasticMask(uint32_t Width, uint32_t Height, int Frame);
    void ApplyBlueNoiseDither();
};

} // namespace Solstice::Render
