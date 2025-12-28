#pragma once

#include <Math/Vector.hxx>
#include <vector>
#include <map>
#include <cstdint>

namespace Solstice::Arzachel {

// Submesh - a group of triangles with the same material
struct SubMesh {
    uint32_t MaterialID;
    uint32_t IndexStart;
    uint32_t IndexCount;

    SubMesh() : MaterialID(0), IndexStart(0), IndexCount(0) {}
    SubMesh(uint32_t MaterialID, uint32_t IndexStart, uint32_t IndexCount)
        : MaterialID(MaterialID), IndexStart(IndexStart), IndexCount(IndexCount) {}
};

// Immutable raw geometry structure
// This is the pure data representation, separate from Render::Mesh
struct MeshData {
    std::vector<Math::Vec3> Positions;
    std::vector<Math::Vec3> Normals;
    std::vector<Math::Vec2> UVs;
    std::vector<uint32_t> Indices;
    std::vector<SubMesh> SubMeshes;

    Math::Vec3 BoundsMin;
    Math::Vec3 BoundsMax;

    std::vector<MeshData> LODLevels;

    MeshData() {
        BoundsMin = Math::Vec3(0, 0, 0);
        BoundsMax = Math::Vec3(0, 0, 0);
    }

    // Calculate bounds from positions
    void CalculateBounds() {
        if (Positions.empty()) {
            BoundsMin = Math::Vec3(0, 0, 0);
            BoundsMax = Math::Vec3(0, 0, 0);
            return;
        }

        BoundsMin = Positions[0];
        BoundsMax = Positions[0];

        for (const auto& Position : Positions) {
            if (Position.x < BoundsMin.x) BoundsMin.x = Position.x;
            if (Position.y < BoundsMin.y) BoundsMin.y = Position.y;
            if (Position.z < BoundsMin.z) BoundsMin.z = Position.z;
            if (Position.x > BoundsMax.x) BoundsMax.x = Position.x;
            if (Position.y > BoundsMax.y) BoundsMax.y = Position.y;
            if (Position.z > BoundsMax.z) BoundsMax.z = Position.z;
        }
    }

    size_t GetVertexCount() const { return Positions.size(); }
    size_t GetTriangleCount() const { return Indices.size() / 3; }
    size_t GetSubMeshCount() const { return SubMeshes.size(); }

    // Edge representation for adjacency
    struct Edge {
        uint32_t V0, V1;
        Edge(uint32_t A, uint32_t B) : V0(A < B ? A : B), V1(A < B ? B : A) {}
        bool operator<(const Edge& Other) const {
            if (V0 != Other.V0) return V0 < Other.V0;
            return V1 < Other.V1;
        }
        bool operator==(const Edge& Other) const { return V0 == Other.V0 && V1 == Other.V1; }
    };

    // Find all unique edges and their adjacent faces
    // Returns a map from Edge to a list of face indices (triangle indices)
    std::map<Edge, std::vector<uint32_t>> FindEdges() const {
        std::map<Edge, std::vector<uint32_t>> AdjacencyMap;
        for (uint32_t I = 0; I < Indices.size(); I += 3) {
            uint32_t FaceIndex = I / 3;
            Edge E1(Indices[I], Indices[I + 1]);
            Edge E2(Indices[I + 1], Indices[I + 2]);
            Edge E3(Indices[I + 2], Indices[I]);

            AdjacencyMap[E1].push_back(FaceIndex);
            AdjacencyMap[E2].push_back(FaceIndex);
            AdjacencyMap[E3].push_back(FaceIndex);
        }
        return AdjacencyMap;
    }
};

} // namespace Solstice::Arzachel
