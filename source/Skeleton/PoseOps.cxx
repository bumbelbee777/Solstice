#include "PoseOps.hxx"

namespace Solstice::Skeleton {

Pose BlendPoses(const Pose& A, const Pose& B, float Weight) {
    Pose Result;
    auto TA = A.GetBoneTransforms();
    auto TB = B.GetBoneTransforms();
    for (auto const& [ID, TransA] : TA) {
        if (TB.count(ID)) {
            const auto& TransB = TB.at(ID);
            BoneTransform BT;
            BT.Translation = TransA.Translation * (1.0f - Weight) + TransB.Translation * Weight;
            BT.Rotation = Math::Quaternion::Slerp(TransA.Rotation, TransB.Rotation, Weight);
            BT.Scale = TransA.Scale * (1.0f - Weight) + TransB.Scale * Weight;
            Result.SetTransform(ID, BT);
        } else {
            Result.SetTransform(ID, TransA);
        }
    }
    return Result;
}

void SolveIK(Pose& CurrentPose, BoneID TipBone, const Math::Vec3& TargetWorldPos, const Skeleton& SkeletonParam) {
    // CCD IK Placeholder
    (void)CurrentPose;
    (void)TipBone;
    (void)TargetWorldPos;
    (void)SkeletonParam;
}

} // namespace Solstice::Skeleton
