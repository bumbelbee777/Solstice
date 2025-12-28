#pragma once

#include "../Solstice.hxx"
#include <Core/SIMD.hxx>
#include <Physics/LightSource.hxx>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <bgfx/bgfx.h>
#include <vector>
#include <array>
#include <cstdint>

namespace Solstice::Render {

// Forward declarations
class Scene;

// Bitplane storage for light occlusion (matches VoxelGrid pattern from Raytracing)
struct SOLSTICE_API LightOcclusionGrid {
    uint32_t ResolutionX = 64;
    uint32_t ResolutionY = 64;
    uint32_t ResolutionZ = 64;
    uint32_t RowsPerY = 1;
    std::vector<uint64_t> Bitmasks; // Flat cache-friendly storage
    Math::Vec3 WorldMin;
    Math::Vec3 WorldMax;
    Math::Vec3 VoxelSize;
    
    void Initialize(const Math::Vec3& InWorldMin, const Math::Vec3& InWorldMax,
                   uint32_t ResX = 64, uint32_t ResY = 64, uint32_t ResZ = 64);
    void WorldToVoxel(const Math::Vec3& WorldPos, int32_t& OutVX, int32_t& OutVY, int32_t& OutVZ) const;
    void MarkOccluder(int32_t VX, int32_t VY, int32_t VZ);
    bool IsOccluded(int32_t VX, int32_t VY, int32_t VZ) const;
    void Clear();
};

// Light contribution packet - SIMD optimized for 4 lights
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
        
    // Add a light to the packet
    void AddLight(const Physics::LightSource& Light);
    
    // Reset packet
    void Reset() { LightCount = 0; }
};

// Volumetric ray march result
struct VolumetricSample {
    float Density = 0.0f;     // 0-1 accumulated light density
    Math::Vec3 Color;         // Accumulated color
    
    VolumetricSample() : Color(0.0f, 0.0f, 0.0f) {}
};

// Volumetric lighting system with optimizations matching Raytracing.cxx
class SOLSTICE_API VolumetricLighting {
public:
    VolumetricLighting();
    ~VolumetricLighting();
    
    void Initialize(uint32_t Width, uint32_t Height,
                   const Math::Vec3& WorldMin, const Math::Vec3& WorldMax);
    void Shutdown();
    
    // Build occlusion grid from scene geometry
    void BuildOcclusionGrid(const Scene& SceneGraph);
    void BuildOcclusionMipmaps();
    
    // Trace volumetric rays (stochastic, tiled, SIMD)
    // Uses 33% coverage + temporal accumulation
    void TraceVolumetricRays(
        const std::vector<Physics::LightSource>& Lights,
        const Math::Vec3& CameraPos,
        const Math::Matrix4& ViewProj);
    
    // Get output texture for compositing
    bgfx::TextureHandle GetVolumetricTexture() const { return m_VolumetricTexture; }
    
    // Settings
    void SetDensity(float Density) { m_Density = Density; }
    void SetDecay(float Decay) { m_Decay = Decay; }
    void SetExposure(float Exposure) { m_Exposure = Exposure; }
    void SetNumSamples(int Samples) { m_NumSamples = std::min(Samples, 64); }
    void SetStochasticRatio(float Ratio) { m_StochasticRatio = Ratio; }
    
    float GetDensity() const { return m_Density; }
    float GetDecay() const { return m_Decay; }
    float GetExposure() const { return m_Exposure; }
    
private:
    // Occlusion grid (bitplane storage like VoxelGrid)
    LightOcclusionGrid m_OcclusionGrid;
    std::vector<LightOcclusionGrid> m_OcclusionMipmaps; // Hierarchical (64→32→16)
    
    // Output buffers (RGB packed as floats)
    std::vector<float> m_VolumetricBufferR;
    std::vector<float> m_VolumetricBufferG;
    std::vector<float> m_VolumetricBufferB;
    std::vector<float> m_PrevBufferR;
    std::vector<float> m_PrevBufferG;
    std::vector<float> m_PrevBufferB;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    
    // Stochastic sampling (reuse raytracing patterns)
    std::vector<float> m_BlueNoiseTexture;       // 64×64 blue noise
    std::vector<bool> m_StochasticMask;
    uint32_t m_BlueNoiseSize = 64;
    uint32_t m_FrameCounter = 0;
    
    // BGFX resources
    bgfx::TextureHandle m_VolumetricTexture = BGFX_INVALID_HANDLE;
    
    // Settings
    float m_Density = 0.8f;
    float m_Decay = 0.96f;
    float m_Exposure = 0.3f;
    int m_NumSamples = 32;
    float m_StochasticRatio = 0.33f;
    int m_TemporalFrames = 3;
    
    // Tiled processing
    static constexpr int TILE_SIZE = 16;
    
    // Internal methods (matching raytracing patterns)
    void GenerateBlueNoise();
    void GenerateStochasticMask(uint32_t Width, uint32_t Height, int Frame);
    void AccumulateTemporal();
    void ApplyBlueNoiseDither();
    void UploadTexture();
    
    // Tiled processing
    void ProcessTile(int TileX, int TileY,
                    const std::vector<LightPacket>& LightPackets,
                    const Math::Vec3& CameraPos,
                    const Math::Matrix4& InvViewProj);
    
    // Hierarchical ray march (coarse-to-fine, early exit)
    VolumetricSample MarchRayHierarchical(
        const Math::Vec3& Origin,
        const Math::Vec3& Direction,
        float MaxDistance,
        const std::vector<LightPacket>& NearestLights);
    
    // Cheap approximations
    float FastAttenuation(float Distance, float Range) const;
    
    // Pack lights into SIMD packets
    void PackLightsIntoPackets(const std::vector<Physics::LightSource>& Lights,
                               std::vector<LightPacket>& OutPackets);
};

} // namespace Solstice::Render
