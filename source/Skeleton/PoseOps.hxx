#pragma once

#include "Skeleton.hxx"

namespace Solstice::Skeleton {

// Blend two poses by weight
Pose BlendPoses(const Pose& A, const Pose& B, float Weight);

// Solve simple two-bone IK (CCD placeholder)
void SolveIK(Pose& CurrentPose, BoneID TipBone, const Math::Vec3& TargetWorldPos, const Skeleton& SkeletonParam);

} // namespace Solstice::Skeleton
