#include "Render/Scene/BatchRenderer.hxx"

namespace Solstice::Render {

void BatchRenderer::BuildBatches(const std::vector<Meshlet>& visibleMeshlets) {
    m_Batches.clear();
    for (const Meshlet& meshlet : visibleMeshlets) {
        BatchKey key{};
        key.MaterialId = meshlet.MaterialId;
        key.MeshletGroupId = meshlet.VertexOffset / 64u;
        key.Depth = meshlet.Center.z;
        m_Batches[key].push_back(&meshlet);
    }
}

void BatchRenderer::DrawBatches() {
    // Batches are consumed by higher-level scene submit paths.
}

} // namespace Solstice::Render
