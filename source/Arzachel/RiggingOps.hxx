#pragma once

#include "Skeleton.hxx"
#include "SkinWeights.hxx"
#include "RiggedMesh.hxx"
#include "Generator.hxx"
#include "AnimationClip.hxx"
#include <map>
#include <string>

namespace Solstice::Arzachel {

// Skeleton merging - combine two skeletons deterministically
Skeleton MergeSkeletons(const Skeleton& A, const Skeleton& B, const std::string& RootName);

// Remap bone references in skin weights
SkinWeights RemapWeights(const SkinWeights& Weights, const std::map<BoneID, BoneID>& BoneMap);

// Merge two rigged meshes
Generator<RiggedMesh> MergeRigged(const Generator<RiggedMesh>& A, const Generator<RiggedMesh>& B);

// Advanced rigging and animation operations
Pose BlendPoses(const Pose& A, const Pose& B, float Weight);

// Solve simple two-bone IK
void SolveIK(Pose& CurrentPose, BoneID TipBone, const Math::Vec3& TargetWorldPos, const Skeleton& SkeletonParam);

} // namespace Solstice::Arzachel
