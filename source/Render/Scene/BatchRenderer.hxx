#pragma once

#include "Render/Assets/MeshletBuilder.hxx"
#include <vector>
#include <map>

namespace Solstice::Render {

struct BatchKey {
    uint32_t MaterialId{0};
    uint32_t MeshletGroupId{0};
    float Depth{0.0f};

    bool operator<(const BatchKey& other) const {
        if (MaterialId != other.MaterialId) return MaterialId < other.MaterialId;
        if (MeshletGroupId != other.MeshletGroupId) return MeshletGroupId < other.MeshletGroupId;
        return Depth < other.Depth;
    }
};

class BatchRenderer {
public:
    void BuildBatches(const std::vector<Meshlet>& visibleMeshlets);
    void DrawBatches();
    size_t GetBatchCount() const { return m_Batches.size(); }

private:
    std::map<BatchKey, std::vector<const Meshlet*>> m_Batches;
};

} // namespace Solstice::Render
