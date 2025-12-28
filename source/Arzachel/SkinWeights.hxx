#pragma once

#include "Skeleton.hxx"
#include <array>
#include <vector>
#include <cstdint>

namespace Solstice::Arzachel {

// Per-vertex bone influence
struct SkinWeight {
    BoneID Id;
    float Weight;

    SkinWeight() : Id(BoneID{}), Weight(0.0f) {}
    SkinWeight(BoneID ID, float WeightParam) : Id(ID), Weight(WeightParam) {}
};

// Per-vertex collection of bone influences (typically 4 bones max)
struct VertexWeights {
    std::array<SkinWeight, 4> Influences;

    VertexWeights() {
        for (auto& Influence : Influences) {
            Influence = SkinWeight{};
        }
    }

    // Normalize weights to sum to 1.0
    void Normalize() {
        float Sum = 0.0f;
        for (const auto& Influence : Influences) {
            Sum += Influence.Weight;
        }
        if (Sum > 0.0f) {
            for (auto& Influence : Influences) {
                Influence.Weight /= Sum;
            }
        }
    }
};

// Collection of skin weights for all vertices
class SkinWeights {
public:
    SkinWeights() = default;

    explicit SkinWeights(size_t VertexCount) {
        PerVertexWeights.resize(VertexCount);
    }

    // Get weights for a vertex
    VertexWeights GetWeights(size_t VertexIndex) const {
        if (VertexIndex >= PerVertexWeights.size()) {
            return VertexWeights{};
        }
        return PerVertexWeights[VertexIndex];
    }

    // Set weights for a vertex
    void SetWeights(size_t VertexIndex, const VertexWeights& Weights) {
        if (VertexIndex >= PerVertexWeights.size()) {
            PerVertexWeights.resize(VertexIndex + 1);
        }
        PerVertexWeights[VertexIndex] = Weights;
        PerVertexWeights[VertexIndex].Normalize();
    }

    // Accessors
    size_t GetVertexCount() const { return PerVertexWeights.size(); }
    const std::vector<VertexWeights>& GetPerVertexWeights() const { return PerVertexWeights; }

private:
    std::vector<VertexWeights> PerVertexWeights;
};

} // namespace Solstice::Arzachel
