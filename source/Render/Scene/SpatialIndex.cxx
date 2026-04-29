#include "Render/Scene/SpatialIndex.hxx"

namespace Solstice::Render {

void SpatialIndex::BuildBSP(const std::vector<Meshlet>& staticMeshlets) {
    m_StaticMeshlets = staticMeshlets;
}

void SpatialIndex::BuildPVS() {
}

void SpatialIndex::BuildLooseOctree(const std::vector<EntityId>& dynamicEntities) {
    m_DynamicEntities = dynamicEntities;
}

void SpatialIndex::QueryVisible(const Camera& camera, std::vector<SceneObjectID>& outVisible) const {
    if (!m_Scene) {
        outVisible.clear();
        return;
    }
    m_Scene->FrustumCull(camera, outVisible);
}

} // namespace Solstice::Render
