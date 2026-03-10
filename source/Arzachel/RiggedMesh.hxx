#pragma once

#include "MeshData.hxx"
#include <Skeleton/Skeleton.hxx>
#include "SkinWeights.hxx"

namespace Solstice::Arzachel {

// Immutable rigged mesh structure
// Combines geometry, skeleton, and skin weights
struct RiggedMesh {
    MeshData Mesh;
    ::Solstice::Skeleton::Skeleton MSkeleton;
    SkinWeights MSkinWeights;

    RiggedMesh() = default;

    RiggedMesh(const MeshData& MeshParam, const ::Solstice::Skeleton::Skeleton& SkeletonParam, const SkinWeights& WeightsParam)
        : Mesh(MeshParam), MSkeleton(SkeletonParam), MSkinWeights(WeightsParam) {
        // Invariant: skin weights must reference bones in skeleton
        // This is validated at construction time in factories
    }

    // Validate that all bone IDs in skin weights exist in skeleton
    bool IsValid() const {
        for (size_t I = 0; I < MSkinWeights.GetVertexCount(); ++I) {
            VertexWeights VW = MSkinWeights.GetWeights(I);
            for (const auto& Influence : VW.Influences) {
                if (Influence.Weight > 0.0f && !MSkeleton.FindBone(Influence.Id)) {
                    return false;
                }
            }
        }
        return true;
    }
};

} // namespace Solstice::Arzachel
