#pragma once

#include "../Solstice.hxx"
#include <Render/Assets/Mesh.hxx>
#include "../Math/Vector.hxx"
#include "Seed.hxx"
#include "ProceduralTexture.hxx"
#include "Constraints.hxx"
#include <vector>
#include <memory>
#include <cstdint>

namespace Solstice::Arzachel {

// Hill data structure
struct HillData {
    Math::Vec3 Position;
    float Radius;
    float Height;
};

// Generate terrain heightmap using deterministic seed system
SOLSTICE_API std::vector<float> GenerateTerrainHeightmap(
    uint32_t Resolution,
    float HeightScale,
    uint32_t SeedVal);

// Create terrain mesh from heightmap
SOLSTICE_API std::unique_ptr<Render::Mesh> CreateTerrainMesh(
    const std::vector<float>& HeightMap,
    uint32_t Resolution,
    float TerrainSize);

// Generate hills with constraints
SOLSTICE_API std::vector<HillData> GenerateHills(
    uint32_t Count,
    float TerrainSize,
    uint32_t SeedVal);

// Generate hills with terrain constraints
SOLSTICE_API std::vector<HillData> GenerateHillsWithConstraints(
    uint32_t Count,
    float TerrainSize,
    const std::vector<float>& Heightmap,
    uint32_t HeightmapResolution,
    const std::vector<VisualObjective>& Objectives,
    uint32_t SeedVal);

// Create hill mesh (ellipsoid)
SOLSTICE_API std::unique_ptr<Render::Mesh> CreateHillMesh(
    float Radius,
    float Height);

// Sample terrain height at a world position using bilinear interpolation
SOLSTICE_API float SampleTerrainHeight(
    const std::vector<float>& Heightmap,
    uint32_t Resolution,
    float TerrainSize,
    const Math::Vec3& WorldPos);

// Optimize terrain mesh (placeholder for future optimization)
SOLSTICE_API void OptimizeTerrainMesh(
    Render::Mesh* Mesh,
    const std::vector<float>& HeightMap,
    uint32_t Resolution,
    float TerrainSize);

// Optimize hill mesh (placeholder)
SOLSTICE_API void OptimizeHillMesh(Render::Mesh* Mesh);

// Create LOD mountain mesh for background elements
SOLSTICE_API std::unique_ptr<Render::Mesh> CreateLODMountainMesh(
    float BaseWidth,
    float Height,
    float PeakWidth,
    const Seed& SeedParam);

} // namespace Solstice::Arzachel
