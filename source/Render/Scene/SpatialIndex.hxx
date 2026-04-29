#pragma once

#include "Render/Assets/MeshletBuilder.hxx"
#include "Render/Scene/Scene.hxx"
#include <vector>
#include <cstdint>

namespace Solstice::Render {

using EntityId = uint32_t;

struct AABB {
    Math::Vec3 Min{0.0f, 0.0f, 0.0f};
    Math::Vec3 Max{0.0f, 0.0f, 0.0f};
};

struct BIHNode {
    float SplitMin[3]{0.0f, 0.0f, 0.0f};
    float SplitMax[3]{0.0f, 0.0f, 0.0f};
    uint32_t LeftChild{0};
    uint32_t RightChild{0};
    uint32_t StartIndex{0};
    uint32_t EndIndex{0};
    uint8_t Axis{0};
    bool IsLeaf{true};
};

class SpatialIndex {
public:
    void BuildBSP(const std::vector<Meshlet>& staticMeshlets);
    void BuildPVS();
    void BuildLooseOctree(const std::vector<EntityId>& dynamicEntities);

    void SetScene(Scene* scene) { m_Scene = scene; }
    void QueryVisible(const Camera& camera, std::vector<SceneObjectID>& outVisible) const;

private:
    Scene* m_Scene{nullptr};
    std::vector<Meshlet> m_StaticMeshlets;
    std::vector<EntityId> m_DynamicEntities;
};

} // namespace Solstice::Render
