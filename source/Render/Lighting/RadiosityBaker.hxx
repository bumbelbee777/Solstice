#pragma once

#include <Solstice.hxx>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <vector>
#include <cstdint>
#include <memory>

namespace Solstice::Render {

// Radiosity patch - represents a surface element for radiosity computation
struct RadiosityPatch {
    Math::Vec3 Position;      // Center position
    Math::Vec3 Normal;         // Surface normal
    Math::Vec3 Color;          // Albedo/diffuse color
    Math::Vec3 Radiosity;      // Accumulated radiosity (lighting)
    float Area;                // Patch area
    float Emissivity;         // Self-emission (for light sources)
    uint32_t MaterialID;       // Material index

    RadiosityPatch() : Area(0.0f), Emissivity(0.0f), MaterialID(0) {}
};

// Radiosity configuration
struct RadiosityConfig {
    uint32_t MaxBounces = 5;           // Maximum number of light bounces
    uint32_t PatchResolution = 64;      // Patches per unit (for subdivision)
    float ConvergenceThreshold = 0.01f; // Energy threshold for convergence
    bool UseHierarchical = false;       // Use hierarchical radiosity
    bool UseSIMD = true;                // Use SIMD optimizations
    uint32_t ThreadCount = 0;           // 0 = auto-detect
};

// Radiosity baker - computes global illumination using progressive radiosity
class SOLSTICE_API RadiosityBaker {
public:
    RadiosityBaker();
    ~RadiosityBaker();

    // Initialize with scene bounds
    void Initialize(const Math::Vec3& WorldMin, const Math::Vec3& WorldMax, const RadiosityConfig& Config = RadiosityConfig());

    // Build patches from scene geometry
    void BuildPatches(class Scene& scene, class MeshLibrary* meshLib, class Core::MaterialLibrary* materialLib);

    // Run radiosity computation
    void ComputeRadiosity(const std::vector<Physics::LightSource>& Lights);

    // Get lightmap data for a material
    // Returns RGBM-encoded lightmap data
    std::vector<uint8_t> GetLightmapData(uint32_t MaterialID, uint32_t Width, uint32_t Height) const;

    // Get radiosity for a specific position (interpolated from patches)
    Math::Vec3 SampleRadiosity(const Math::Vec3& Position, const Math::Vec3& Normal) const;

    // Configuration
    void SetConfig(const RadiosityConfig& Config) { m_config = Config; }
    const RadiosityConfig& GetConfig() const { return m_config; }

    // Statistics
    struct Stats {
        uint32_t PatchCount;
        uint32_t Iterations;
        float TotalEnergy;
        float ComputationTimeMs;
    };
    Stats GetStats() const { return m_stats; }

private:
    // Progressive radiosity iteration
    void ProgressiveRadiosityIteration();

    // Calculate form factor between two patches
    float CalculateFormFactor(const RadiosityPatch& PatchA, const RadiosityPatch& PatchB) const;

    // SIMD-accelerated form factor calculation (4 patches at once)
    void CalculateFormFactorsSIMD(const RadiosityPatch& SourcePatch,
                                   const RadiosityPatch* TargetPatches,
                                   float* OutFormFactors,
                                   size_t Count) const;

    // Subdivide surface into patches
    void SubdivideSurface(const Math::Vec3& V0, const Math::Vec3& V1, const Math::Vec3& V2,
                          const Math::Vec3& Normal, const Math::Vec3& Color, float Emissivity,
                          uint32_t MaterialID, uint32_t Depth);

    RadiosityConfig m_config;
    std::vector<RadiosityPatch> m_patches;
    Math::Vec3 m_worldMin;
    Math::Vec3 m_worldMax;
    bool m_initialized;
    Stats m_stats;
};

} // namespace Solstice::Render

