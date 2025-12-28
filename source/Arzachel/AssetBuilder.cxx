#include "AssetBuilder.hxx"
#include "MeshData.hxx"
#include <Render/Mesh.hxx>
#include <Core/Debug.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Arzachel {

std::unique_ptr<Render::Mesh> ConvertToRenderMesh(const MeshData& MeshDataParam) {
    auto RenderMesh = std::make_unique<Render::Mesh>();

    if (MeshDataParam.Positions.empty() || MeshDataParam.Indices.empty()) {
        SIMPLE_LOG("WARNING: Converting empty MeshData to Render::Mesh");
        return RenderMesh;
    }

    // Validate sizes match
    size_t VertexCount = MeshDataParam.Positions.size();
    if (MeshDataParam.Normals.size() != VertexCount || MeshDataParam.UVs.size() != VertexCount) {
        SIMPLE_LOG("ERROR: MeshData vertex attribute sizes don't match!");
        return nullptr;
    }

    // Convert vertices
    RenderMesh->Vertices.reserve(VertexCount);
    for (size_t I = 0; I < VertexCount; ++I) {
        const Math::Vec3& Position = MeshDataParam.Positions[I];
        const Math::Vec3& Normal = (I < MeshDataParam.Normals.size()) ? MeshDataParam.Normals[I] : Math::Vec3(0, 1, 0);
        const Math::Vec2& UV = (I < MeshDataParam.UVs.size()) ? MeshDataParam.UVs[I] : Math::Vec2(0, 0);

        RenderMesh->AddVertex(Position, Normal, UV);
    }

    // Convert indices
    RenderMesh->Indices = MeshDataParam.Indices;

    // Convert submeshes
    for (const auto& SubMeshObj : MeshDataParam.SubMeshes) {
        RenderMesh->AddSubMesh(SubMeshObj.MaterialID, SubMeshObj.IndexStart, SubMeshObj.IndexCount);
    }

    // Copy bounds
    RenderMesh->BoundsMin = MeshDataParam.BoundsMin;
    RenderMesh->BoundsMax = MeshDataParam.BoundsMax;

    return RenderMesh;
}

bool ValidateMesh(const MeshData& MeshDataParam) {
    // Check for empty mesh
    if (MeshDataParam.Positions.empty()) {
        SIMPLE_LOG("ERROR: MeshData has no positions");
        return false;
    }

    if (MeshDataParam.Indices.empty()) {
        SIMPLE_LOG("ERROR: MeshData has no indices");
        return false;
    }

    // Check vertex attribute sizes match
    size_t VertexCount = MeshDataParam.Positions.size();
    if (MeshDataParam.Normals.size() != VertexCount) {
        SIMPLE_LOG("ERROR: MeshData normals count doesn't match positions count");
        return false;
    }

    if (MeshDataParam.UVs.size() != VertexCount) {
        SIMPLE_LOG("ERROR: MeshData UVs count doesn't match positions count");
        return false;
    }

    // Check indices are valid
    for (uint32_t Index : MeshDataParam.Indices) {
        if (Index >= VertexCount) {
            SIMPLE_LOG("ERROR: MeshData has invalid index: " + std::to_string(Index) + " >= " + std::to_string(VertexCount));
            return false;
        }
    }

    // Check indices form triangles (multiple of 3)
    if (MeshDataParam.Indices.size() % 3 != 0) {
        SIMPLE_LOG("ERROR: MeshData indices count is not a multiple of 3");
        return false;
    }

    // Check submeshes are valid
    for (const auto& SubMeshObj : MeshDataParam.SubMeshes) {
        if (SubMeshObj.IndexStart + SubMeshObj.IndexCount > MeshDataParam.Indices.size()) {
            SIMPLE_LOG("ERROR: MeshData submesh extends beyond indices");
            return false;
        }
    }

    // Check bounds are reasonable
    if (MeshDataParam.BoundsMin.x > MeshDataParam.BoundsMax.x ||
        MeshDataParam.BoundsMin.y > MeshDataParam.BoundsMax.y ||
        MeshDataParam.BoundsMin.z > MeshDataParam.BoundsMax.z) {
        SIMPLE_LOG("ERROR: MeshData has invalid bounds");
        return false;
    }

    // Check for NaN or infinite values
    for (const auto& Position : MeshDataParam.Positions) {
        if (!std::isfinite(Position.x) || !std::isfinite(Position.y) || !std::isfinite(Position.z)) {
            SIMPLE_LOG("ERROR: MeshData has NaN or infinite position");
            return false;
        }
    }

    return true;
}

bool ValidateMesh(const Render::Mesh& MeshParam) {
    // Check for empty mesh
    if (MeshParam.Vertices.empty()) {
        SIMPLE_LOG("ERROR: Render::Mesh has no vertices");
        return false;
    }

    if (MeshParam.Indices.empty()) {
        SIMPLE_LOG("ERROR: Render::Mesh has no indices");
        return false;
    }

    // Check indices are valid
    size_t VertexCount = MeshParam.Vertices.size();
    for (uint32_t Index : MeshParam.Indices) {
        if (Index >= VertexCount) {
            SIMPLE_LOG("ERROR: Render::Mesh has invalid index: " + std::to_string(Index) + " >= " + std::to_string(VertexCount));
            return false;
        }
    }

    // Check indices form triangles (multiple of 3)
    if (MeshParam.Indices.size() % 3 != 0) {
        SIMPLE_LOG("ERROR: Render::Mesh indices count is not a multiple of 3");
        return false;
    }

    // Check submeshes are valid
    for (const auto& SubMeshObj : MeshParam.SubMeshes) {
        if (SubMeshObj.IndexStart + SubMeshObj.IndexCount > MeshParam.Indices.size()) {
            SIMPLE_LOG("ERROR: Render::Mesh submesh extends beyond indices");
            return false;
        }
    }

    // Check bounds are reasonable
    if (MeshParam.BoundsMin.x > MeshParam.BoundsMax.x ||
        MeshParam.BoundsMin.y > MeshParam.BoundsMax.y ||
        MeshParam.BoundsMin.z > MeshParam.BoundsMax.z) {
        SIMPLE_LOG("ERROR: Render::Mesh has invalid bounds");
        return false;
    }

    // Check for NaN or infinite values
    for (const auto& Vertex : MeshParam.Vertices) {
        if (!std::isfinite(Vertex.PosX) || !std::isfinite(Vertex.PosY) || !std::isfinite(Vertex.PosZ)) {
            SIMPLE_LOG("ERROR: Render::Mesh has NaN or infinite position");
            return false;
        }
    }

    return true;
}

CompleteAsset BuildCompleteAsset(
    const Generator<MeshData>& MeshGenerator,
    const Seed& SeedParam) {

    CompleteAsset Asset;

    // Generate mesh data
    MeshData MeshDataResult = MeshGenerator(SeedParam);

    // Validate mesh
    if (!ValidateMesh(MeshDataResult)) {
        SIMPLE_LOG("ERROR: Generated mesh data failed validation");
        return Asset; // Return empty asset
    }

    // Convert to render mesh
    Asset.Mesh = ConvertToRenderMesh(MeshDataResult);

    // Validate render mesh
    if (Asset.Mesh && !ValidateMesh(*Asset.Mesh)) {
        SIMPLE_LOG("ERROR: Converted render mesh failed validation");
        Asset.Mesh.reset();
    }

    return Asset;
}

} // namespace Solstice::Arzachel
