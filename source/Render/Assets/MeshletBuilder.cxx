#include "Render/Assets/MeshletBuilder.hxx"
#include <algorithm>

namespace Solstice::Render {

uint32_t MeshletLibrary::GetOrAddMeshlet(const Meshlet& meshlet) {
    const uint64_t hash = (static_cast<uint64_t>(meshlet.VertexOffset) << 32u) | meshlet.IndexOffset;
    auto it = HashToIndex.find(hash);
    if (it != HashToIndex.end()) {
        return it->second;
    }
    const uint32_t index = static_cast<uint32_t>(UniqueMeshlets.size());
    UniqueMeshlets.push_back(meshlet);
    HashToIndex[hash] = index;
    return index;
}

MeshletLibrary MeshletBuilder::BuildMeshlets(const std::vector<QuantizedVertex>& vertices,
                                             const std::vector<uint32_t>& indices,
                                             uint32_t maxVerticesPerMeshlet,
                                             uint32_t maxIndicesPerMeshlet) {
    MeshletLibrary library;
    if (vertices.empty() || indices.empty()) {
        return library;
    }

    const uint32_t step = std::max<uint32_t>(3u, (maxIndicesPerMeshlet / 3u) * 3u);
    for (uint32_t indexOffset = 0; indexOffset < indices.size(); indexOffset += step) {
        Meshlet meshlet{};
        meshlet.IndexOffset = indexOffset;
        meshlet.IndexCount = std::min<uint32_t>(step, static_cast<uint32_t>(indices.size()) - indexOffset);
        meshlet.VertexOffset = 0u;
        meshlet.VertexCount = std::min<uint32_t>(maxVerticesPerMeshlet, static_cast<uint32_t>(vertices.size()));

        Math::Vec3 minP(1e30f, 1e30f, 1e30f);
        Math::Vec3 maxP(-1e30f, -1e30f, -1e30f);
        for (uint32_t i = 0; i < meshlet.IndexCount; ++i) {
            const uint32_t idx = indices[indexOffset + i];
            if (idx >= vertices.size()) {
                continue;
            }
            const Math::Vec3 p(vertices[idx].PosX, vertices[idx].PosY, vertices[idx].PosZ);
            minP.x = std::min(minP.x, p.x); minP.y = std::min(minP.y, p.y); minP.z = std::min(minP.z, p.z);
            maxP.x = std::max(maxP.x, p.x); maxP.y = std::max(maxP.y, p.y); maxP.z = std::max(maxP.z, p.z);
        }
        meshlet.Center = (minP + maxP) * 0.5f;
        meshlet.Radius = (maxP - meshlet.Center).Length();
        ComputeBoundingCone(meshlet);
        ComputeLodError(meshlet);
        library.GetOrAddMeshlet(meshlet);
    }

    return library;
}

void MeshletBuilder::ComputeBoundingCone(Meshlet& meshlet) {
    meshlet.Cone.AxisX = 0.0f;
    meshlet.Cone.AxisY = 0.0f;
    meshlet.Cone.AxisZ = 1.0f;
    meshlet.Cone.Cutoff = -0.25f;
}

void MeshletBuilder::ComputeLodError(Meshlet& meshlet) {
    meshlet.LodError = meshlet.Radius * 0.01f;
}

uint64_t MeshletBuilder::HashMeshlet(const Meshlet& meshlet) {
    return (static_cast<uint64_t>(meshlet.VertexOffset) << 32u) ^ static_cast<uint64_t>(meshlet.IndexOffset);
}

} // namespace Solstice::Render
