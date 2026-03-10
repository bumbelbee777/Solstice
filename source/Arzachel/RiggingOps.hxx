#pragma once

#include <Skeleton/Skeleton.hxx>
#include "SkinWeights.hxx"
#include "RiggedMesh.hxx"
#include "Generator.hxx"
#include "AnimationClip.hxx"
#include <map>
#include <string>

namespace Solstice::Arzachel {

// Skeleton merging - combine two skeletons deterministically
::Solstice::Skeleton::Skeleton MergeSkeletons(const ::Solstice::Skeleton::Skeleton& A, const ::Solstice::Skeleton::Skeleton& B, const std::string& RootName);

// Remap bone references in skin weights
SkinWeights RemapWeights(const SkinWeights& Weights, const std::map<::Solstice::Skeleton::BoneID, ::Solstice::Skeleton::BoneID>& BoneMap);

// Merge two rigged meshes
Generator<RiggedMesh> MergeRigged(const Generator<RiggedMesh>& A, const Generator<RiggedMesh>& B);

} // namespace Solstice::Arzachel
