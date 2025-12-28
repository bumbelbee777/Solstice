#include "TerrainGenerator.hxx"
#include "MaterialPresets.hxx"
#include "Polyhedra.hxx"
#include "Constraints.hxx"
#include "GeometryOps.hxx"
#include "AssetBuilder.hxx"
#include "MeshData.hxx"
#include "../Render/Mesh.hxx"
#include "../Core/Debug.hxx"
#include <algorithm>
#include <cmath>

namespace Solstice::Arzachel {

std::vector<float> GenerateTerrainHeightmap(
    uint32_t Resolution,
    float HeightScale,
    uint32_t SeedVal) {

    Seed ArzSeed(SeedVal);
    std::vector<float> HeightMap(Resolution * Resolution);

    // Use Arzachel's noise for consistency
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            float Nx = static_cast<float>(X) / static_cast<float>(Resolution);
            float Ny = static_cast<float>(Y) / static_cast<float>(Resolution);
            float Noise = ProceduralTexture::PerlinNoise2D(Nx, Ny, 6, 4.0f, ArzSeed);
            HeightMap[Y * Resolution + X] = Noise * HeightScale;
        }
    }

    return HeightMap;
}

std::unique_ptr<Render::Mesh> CreateTerrainMesh(
    const std::vector<float>& HeightMap,
    uint32_t Resolution,
    float TerrainSize) {

    auto MeshPtr = std::make_unique<Render::Mesh>();
    const float CellSize = TerrainSize / static_cast<float>(Resolution - 1);
    const float HalfSize = TerrainSize * 0.5f;

    for (uint32_t Y = 0; Y < Resolution; ++Y) {
        for (uint32_t X = 0; X < Resolution; ++X) {
            float WorldX = -HalfSize + static_cast<float>(X) * CellSize;
            float WorldZ = -HalfSize + static_cast<float>(Y) * CellSize;
            float WorldY = HeightMap[Y * Resolution + X];

            Math::Vec3 Pos(WorldX, WorldY, WorldZ);
            Math::Vec3 Normal(0, 1, 0);
            Math::Vec2 UV(static_cast<float>(X) / Resolution, static_cast<float>(Y) / Resolution);

            MeshPtr->AddVertex(Pos, Normal, UV);
        }
    }

    // Lake parameters
    const float CenterX = 0.0f;
    const float CenterZ = 0.0f;
    const float LakeRadius = 0.25f;
    const float LakeRadiusWorld = LakeRadius * TerrainSize;

    std::vector<uint32_t> LakeIndices;
    std::vector<uint32_t> TerrainIndices;

    // Generate triangles and classify them
    for (uint32_t Y = 0; Y < Resolution - 1; ++Y) {
        for (uint32_t X = 0; X < Resolution - 1; ++X) {
            uint32_t I0 = Y * Resolution + X;
            uint32_t I1 = Y * Resolution + (X + 1);
            uint32_t I2 = (Y + 1) * Resolution + X;
            uint32_t I3 = (Y + 1) * Resolution + (X + 1);

            auto IsCoordInLake = [&](uint32_t CurX, uint32_t CurY) {
                float WorldX = -HalfSize + static_cast<float>(CurX) * CellSize;
                float WorldZ = -HalfSize + static_cast<float>(CurY) * CellSize;
                float Dx = WorldX - CenterX;
                float Dz = WorldZ - CenterZ;
                float DistFromCenter = std::sqrt(Dx * Dx + Dz * Dz);
                return DistFromCenter < LakeRadiusWorld;
            };

            bool Tri0InLake = IsCoordInLake(X, Y) && IsCoordInLake(X, Y+1) && IsCoordInLake(X+1, Y);
            bool Tri1InLake = IsCoordInLake(X+1, Y) && IsCoordInLake(X, Y+1) && IsCoordInLake(X+1, Y+1);

            if (Tri0InLake) {
                LakeIndices.push_back(I0); LakeIndices.push_back(I2); LakeIndices.push_back(I1);
            } else {
                TerrainIndices.push_back(I0); TerrainIndices.push_back(I2); TerrainIndices.push_back(I1);
            }

            if (Tri1InLake) {
                LakeIndices.push_back(I1); LakeIndices.push_back(I2); LakeIndices.push_back(I3);
            } else {
                TerrainIndices.push_back(I1); TerrainIndices.push_back(I2); TerrainIndices.push_back(I3);
            }
        }
    }

    // Build final index buffer
    MeshPtr->Indices.clear();
    MeshPtr->Indices.reserve(LakeIndices.size() + TerrainIndices.size());
    MeshPtr->Indices.insert(MeshPtr->Indices.end(), LakeIndices.begin(), LakeIndices.end());
    MeshPtr->Indices.insert(MeshPtr->Indices.end(), TerrainIndices.begin(), TerrainIndices.end());

    // Calculate bounds
    float MinY = *std::min_element(HeightMap.begin(), HeightMap.end());
    float MaxY = *std::max_element(HeightMap.begin(), HeightMap.end());
    MeshPtr->BoundsMin = Math::Vec3(-HalfSize, MinY, -HalfSize);
    MeshPtr->BoundsMax = Math::Vec3(HalfSize, MaxY, HalfSize);

    // Create submeshes
    uint32_t LakeIndexStart = 0;
    uint32_t LakeIndexCount = static_cast<uint32_t>(LakeIndices.size());
    uint32_t TerrainIndexStart = LakeIndexCount;
    uint32_t TerrainIndexCount = static_cast<uint32_t>(TerrainIndices.size());

    if (LakeIndexCount > 0)
        MeshPtr->AddSubMesh(0, LakeIndexStart, LakeIndexCount);
    if (TerrainIndexCount > 0)
        MeshPtr->AddSubMesh(1, TerrainIndexStart, TerrainIndexCount);

    return MeshPtr;
}

std::vector<HillData> GenerateHills(uint32_t Count, float TerrainSize, uint32_t SeedVal) {
    std::vector<HillData> Hills;
    Seed ArzSeed(SeedVal);

    for (uint32_t I = 0; I < Count; ++I) {
        Seed HillSeed = ArzSeed.Derive(I);
        float X = (static_cast<float>(HillSeed.Derive(0).Value % 1000) / 1000.0f) * TerrainSize - TerrainSize * 0.5f;
        float Z = (static_cast<float>(HillSeed.Derive(1).Value % 1000) / 1000.0f) * TerrainSize - TerrainSize * 0.5f;
        float Radius = 5.0f + (static_cast<float>(HillSeed.Derive(2).Value % 1000) / 1000.0f) * 15.0f;
        float Height = 3.0f + (static_cast<float>(HillSeed.Derive(3).Value % 1000) / 1000.0f) * 10.0f;

        Hills.push_back({Math::Vec3(X, 0.0f, Z), Radius, Height});
    }
    return Hills;
}

std::vector<HillData> GenerateHillsWithConstraints(
    uint32_t Count,
    float TerrainSize,
    const std::vector<float>& Heightmap,
    uint32_t HeightmapResolution,
    const std::vector<VisualObjective>& Objectives,
    uint32_t SeedVal) {

    std::vector<HillData> Hills;
    Seed ArzSeed(SeedVal);
    TerrainConstraint Terrain(&Heightmap, HeightmapResolution, TerrainSize, -100.0f, 1000.0f);

    for (uint32_t I = 0; I < Count; ++I) {
        Seed HillSeed = ArzSeed.Derive(I);

        ConstraintResult Result;
        if (!Objectives.empty()) {
            Result = SatisfyVisualObjective(Objectives[I % Objectives.size()], Terrain, Math::Vec3(0, 0, 0), HillSeed);
        } else {
            float X = (static_cast<float>(HillSeed.Derive(0).Value % 1000) / 1000.0f) * TerrainSize - TerrainSize * 0.5f;
            float Z = (static_cast<float>(HillSeed.Derive(1).Value % 1000) / 1000.0f) * TerrainSize - TerrainSize * 0.5f;
            Result = SatisfyTerrainConstraint(Terrain, Math::Vec3(X, 0, Z));
        }

        if (Result.Satisfied) {
            float Radius = 5.0f + (static_cast<float>(HillSeed.Derive(2).Value % 1000) / 1000.0f) * 15.0f;
            float Height = 3.0f + (static_cast<float>(HillSeed.Derive(3).Value % 1000) / 1000.0f) * 10.0f;
            Hills.push_back({Result.FinalPosition, Radius, Height});
        }
    }

    return Hills;
}

std::unique_ptr<Render::Mesh> CreateHillMesh(float Radius, float Height) {
    Seed SeedVal(12345);
    auto SphereGen = Sphere(1.0f, 16, SeedVal);
    auto HillGen = Scale(SphereGen, Math::Vec3(Radius, Height, Radius));

    MeshData Data = HillGen(SeedVal);
    return ConvertToRenderMesh(Data);
}

std::unique_ptr<Render::Mesh> CreateLODMountainMesh(
    float BaseWidth,
    float Height,
    float PeakWidth,
    const Seed& SeedParam) {

    // Generate simple low-poly mountain (pyramidal/triangular shape)
    // Use noise for peak position variation
    float PeakOffsetX = (ProceduralTexture::PerlinNoise2D(0.0f, 0.0f, 1, 1.0f, SeedParam) * 0.3f) * BaseWidth;
    float PeakOffsetZ = (ProceduralTexture::PerlinNoise2D(1.0f, 0.0f, 1, 1.0f, SeedParam) * 0.3f) * BaseWidth;

    float HalfBase = BaseWidth * 0.5f;

    // Create MeshData (Arzachel format, independent of renderer)
    MeshData MountainData;

    // Base vertices (square base)
    MountainData.Positions = {
        Math::Vec3(-HalfBase, 0.0f, -HalfBase),  // 0: Base corner
        Math::Vec3(HalfBase, 0.0f, -HalfBase),   // 1: Base corner
        Math::Vec3(HalfBase, 0.0f, HalfBase),     // 2: Base corner
        Math::Vec3(-HalfBase, 0.0f, HalfBase),   // 3: Base corner
        Math::Vec3(PeakOffsetX, Height, PeakOffsetZ)  // 4: Peak
    };

    // Initialize normals and UVs
    MountainData.Normals.resize(5, Math::Vec3(0, 1, 0));
    MountainData.UVs = {
        Math::Vec2(0, 0), Math::Vec2(1, 0),
        Math::Vec2(1, 1), Math::Vec2(0, 1),
        Math::Vec2(0.5f, 0.5f)  // Peak UV
    };

    // Indices for 4 triangular faces (sides of pyramid) + base
    MountainData.Indices = {
        // Face 1: 0-1-4
        0, 1, 4,
        // Face 2: 1-2-4
        1, 2, 4,
        // Face 3: 2-3-4
        2, 3, 4,
        // Face 4: 3-0-4
        3, 0, 4,
        // Base: 0-2-1 and 0-3-2
        0, 2, 1,
        0, 3, 2
    };

    // Calculate proper normals for each face
    for (size_t i = 0; i < MountainData.Indices.size(); i += 3) {
        uint32_t i0 = MountainData.Indices[i];
        uint32_t i1 = MountainData.Indices[i + 1];
        uint32_t i2 = MountainData.Indices[i + 2];

        Math::Vec3 v0 = MountainData.Positions[i0];
        Math::Vec3 v1 = MountainData.Positions[i1];
        Math::Vec3 v2 = MountainData.Positions[i2];

        Math::Vec3 edge1 = v1 - v0;
        Math::Vec3 edge2 = v2 - v0;
        Math::Vec3 normal = edge1.Cross(edge2).Normalized();

        // Set normal for each vertex
        MountainData.Normals[i0] = normal;
        MountainData.Normals[i1] = normal;
        MountainData.Normals[i2] = normal;
    }

    // Calculate bounds
    MountainData.CalculateBounds();

    // Add single submesh
    MountainData.SubMeshes.push_back(SubMesh(0, 0, static_cast<uint32_t>(MountainData.Indices.size())));

    // Convert MeshData to Render::Mesh using AssetBuilder
    return ConvertToRenderMesh(MountainData);
}

float SampleTerrainHeight(
    const std::vector<float>& Heightmap,
    uint32_t Resolution,
    float TerrainSize,
    const Math::Vec3& WorldPos) {

    float HalfSize = TerrainSize * 0.5f;
    float GridX = (WorldPos.x + HalfSize) / TerrainSize * (Resolution - 1);
    float GridZ = (WorldPos.z + HalfSize) / TerrainSize * (Resolution - 1);

    int X0 = std::clamp(static_cast<int>(std::floor(GridX)), 0, static_cast<int>(Resolution - 1));
    int Z0 = std::clamp(static_cast<int>(std::floor(GridZ)), 0, static_cast<int>(Resolution - 1));
    int X1 = std::clamp(X0 + 1, 0, static_cast<int>(Resolution - 1));
    int Z1 = std::clamp(Z0 + 1, 0, static_cast<int>(Resolution - 1));

    float Tx = GridX - std::floor(GridX);
    float Tz = GridZ - std::floor(GridZ);

    float H00 = Heightmap[Z0 * Resolution + X0];
    float H10 = Heightmap[Z0 * Resolution + X1];
    float H01 = Heightmap[Z1 * Resolution + X0];
    float H11 = Heightmap[Z1 * Resolution + X1];

    float H0 = H00 * (1.0f - Tx) + H10 * Tx;
    float H1 = H01 * (1.0f - Tx) + H11 * Tx;

    return H0 * (1.0f - Tz) + H1 * Tz;
}

void OptimizeTerrainMesh(Render::Mesh* Mesh, const std::vector<float>& HeightMap, uint32_t Resolution, float TerrainSize) {
    if (!Mesh || Mesh->Vertices.empty()) return;
    // No-op for now, simplified
}

void OptimizeHillMesh(Render::Mesh* Mesh) {
    // No-op
}

} // namespace Solstice::Arzachel
