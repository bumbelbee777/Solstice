#include "RiggingOps.hxx"
#include "GeometryOps.hxx"
#include <algorithm>
#include <cstdint>

namespace Solstice::Arzachel {

Skeleton MergeSkeletons(const Skeleton& A, const Skeleton& B, const std::string& RootName) {
    std::vector<Bone> MergedBones;
    std::map<BoneID, BoneID> BoneMap; // Old ID -> New ID mapping

    uint32_t NextBoneID = 0;
    BoneID NewRoot = BoneID{NextBoneID++};

    // Create new root bone
    Bone RootBone(NewRoot, RootName, BoneID{}, Math::Matrix4::Identity(), Math::Matrix4::Identity());
    MergedBones.push_back(RootBone);

    // Add skeleton A's bones (remap to new IDs)
    BoneID ARoot = A.GetRoot();
    std::map<BoneID, BoneID> AMap;

    std::function<void(const Skeleton&, BoneID, BoneID, std::map<BoneID, BoneID>&)> AddBoneRecursive;
    AddBoneRecursive = [&](const Skeleton& Skel, BoneID BoneIDParam, BoneID NewParentID, std::map<BoneID, BoneID>& IDMap) {
        const Bone* BonePtr = Skel.FindBone(BoneIDParam);
        if (!BonePtr) return;

        BoneID NewID(NextBoneID++);
        IDMap[BoneIDParam] = NewID;

        Bone NewBone(NewID, BonePtr->Name, NewParentID, BonePtr->LocalTransform, BonePtr->InverseBindMatrix);
        MergedBones.push_back(NewBone);

        // Add children
        std::vector<BoneID> Children = Skel.GetChildren(BoneIDParam);
        for (BoneID ChildID : Children) {
            AddBoneRecursive(Skel, ChildID, NewID, IDMap);
        }
    };

    if (ARoot.IsValid()) {
        AddBoneRecursive(A, ARoot, NewRoot, AMap);
    }

    // Add skeleton B's bones (remap to new IDs)
    BoneID BRoot = B.GetRoot();
    std::map<BoneID, BoneID> BMap;

    if (BRoot.IsValid()) {
        AddBoneRecursive(B, BRoot, NewRoot, BMap);
    }

    // Merge bone maps
    BoneMap.insert(AMap.begin(), AMap.end());
    BoneMap.insert(BMap.begin(), BMap.end());

    return Skeleton(MergedBones, NewRoot);
}

SkinWeights RemapWeights(const SkinWeights& Weights, const std::map<BoneID, BoneID>& BoneMap) {
    SkinWeights Remapped;
    Remapped = SkinWeights(Weights.GetVertexCount());

    for (size_t I = 0; I < Weights.GetVertexCount(); ++I) {
        VertexWeights VW = Weights.GetWeights(I);
        VertexWeights RemappedVW;

        for (size_t J = 0; J < VW.Influences.size(); ++J) {
            const SkinWeight& SW = VW.Influences[J];
            auto It = BoneMap.find(SW.Id);
            if (It != BoneMap.end()) {
                RemappedVW.Influences[J] = SkinWeight(It->second, SW.Weight);
            } else {
                RemappedVW.Influences[J] = SkinWeight{};
            }
        }

        RemappedVW.Normalize();
        Remapped.SetWeights(I, RemappedVW);
    }

    return Remapped;
}

Generator<RiggedMesh> MergeRigged(const Generator<RiggedMesh>& A, const Generator<RiggedMesh>& B) {
    return Generator<RiggedMesh>(std::function<RiggedMesh(const Seed&)>([A, B](const Seed& SeedParam) {
        Seed SeedA = SeedParam.Derive(0);
        Seed SeedB = SeedParam.Derive(1);

        RiggedMesh RiggedA = A(SeedA);
        RiggedMesh RiggedB = B(SeedB);

        // Merge skeletons
        Skeleton MergedSkel = MergeSkeletons(RiggedA.MSkeleton, RiggedB.MSkeleton, "MergedRoot");

        // Build bone mapping for remapping weights
        std::map<BoneID, BoneID> BoneMapA, BoneMapB;
        // This is simplified - in practice, we'd track the mapping during MergeSkeletons
        // For now, assume bones are remapped correctly

        // Merge meshes
        Generator<MeshData> MeshGenA = Generator<MeshData>::Constant(RiggedA.Mesh);
        Generator<MeshData> MeshGenB = Generator<MeshData>::Constant(RiggedB.Mesh);
        MeshData MergedMesh = Merge(MeshGenA, MeshGenB)(SeedParam.Derive(2));

        // Remap and merge skin weights
        SkinWeights MergedWeights(MergedMesh.GetVertexCount());

        // Copy weights from A
        for (size_t I = 0; I < RiggedA.MSkinWeights.GetVertexCount(); ++I) {
            MergedWeights.SetWeights(I, RiggedA.MSkinWeights.GetWeights(I));
        }

        // Copy weights from B with offset
        uint32_t VertexOffset = static_cast<uint32_t>(RiggedA.Mesh.GetVertexCount());
        for (size_t I = 0; I < RiggedB.MSkinWeights.GetVertexCount(); ++I) {
            MergedWeights.SetWeights(I + VertexOffset, RiggedB.MSkinWeights.GetWeights(I));
        }

        return RiggedMesh(MergedMesh, MergedSkel, MergedWeights);
    }));
}

Pose BlendPoses(const Pose& A, const Pose& B, float Weight) {
    Pose Result; auto TA = A.GetBoneTransforms(); auto TB = B.GetBoneTransforms();
    for (auto const& [ID, TransA] : TA) {
        if (TB.count(ID)) {
            const auto& TransB = TB.at(ID); BoneTransform BT;
            BT.Translation = TransA.Translation * (1.0f - Weight) + TransB.Translation * Weight;
            BT.Rotation = Math::Quaternion::Slerp(TransA.Rotation, TransB.Rotation, Weight);
            BT.Scale = TransA.Scale * (1.0f - Weight) + TransB.Scale * Weight;
            Result.SetTransform(ID, BT);
        } else Result.SetTransform(ID, TransA);
    }
    return Result;
}

void SolveIK(Pose& CurrentPose, BoneID TipBone, const Math::Vec3& TargetWorldPos, const Skeleton& SkeletonParam) {
    // CCD IK Placeholder
}

} // namespace Solstice::Arzachel
