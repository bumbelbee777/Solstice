#pragma once

#include "Math/Vector.hxx"
#include "Render/Assets/Mesh.hxx"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Solstice::Render {

struct MeshletCone {
    float AxisX{0.0f}, AxisY{0.0f}, AxisZ{1.0f};
    float Cutoff{-1.0f};
};

struct Meshlet {
    uint32_t VertexOffset{0};
    uint32_t IndexOffset{0};
    uint32_t VertexCount{0};
    uint32_t IndexCount{0};
    Math::Vec3 Center{0.0f, 0.0f, 0.0f};
    float Radius{0.0f};
    MeshletCone Cone{};
    uint8_t LodMask{0x1};
    uint32_t MaterialId{0};
    float LodError{0.0f};
};

struct MeshletLibrary {
    std::vector<Meshlet> UniqueMeshlets;
    std::unordered_map<uint64_t, uint32_t> HashToIndex;

    uint32_t GetOrAddMeshlet(const Meshlet& meshlet);
};

class MeshletBuilder {
public:
    MeshletLibrary BuildMeshlets(const std::vector<QuantizedVertex>& vertices,
                                 const std::vector<uint32_t>& indices,
                                 uint32_t maxVerticesPerMeshlet = 64,
                                 uint32_t maxIndicesPerMeshlet = 128);

private:
    void ComputeBoundingCone(Meshlet& meshlet);
    void ComputeLodError(Meshlet& meshlet);
    uint64_t HashMeshlet(const Meshlet& meshlet);
};

} // namespace Solstice::Render
