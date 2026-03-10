#include "RiggingOps.hxx"
#include "GeometryOps.hxx"
#include <Skeleton/PoseOps.hxx>
#include <algorithm>
#include <cstdint>
#include <functional>

namespace Solstice::Arzachel {

using namespace ::Solstice::Skeleton;
using SkeletonTree = ::Solstice::Skeleton::Skeleton;

SkeletonTree MergeSkeletons(const SkeletonTree& A, const SkeletonTree& B, const std::string& RootName) {
    std::vector<Bone> MergedBones;
    std::map<BoneID, BoneID> BoneMap;

    uint32_t NextBoneID = 0;
    BoneID NewRoot = BoneID{NextBoneID++};

    Bone RootBone(NewRoot, RootName, BoneID{}, Math::Matrix4::Identity(), Math::Matrix4::Identity());
    MergedBones.push_back(RootBone);

    BoneID ARoot = A.GetRoot();
    std::map<BoneID, BoneID> AMap;

    std::function<void(const SkeletonTree&, BoneID, BoneID, std::map<BoneID, BoneID>&)> AddBoneRecursive;
    AddBoneRecursive = [&](const SkeletonTree& Skel, BoneID BoneIDParam, BoneID NewParentID, std::map<BoneID, BoneID>& IDMap) {
        const Bone* BonePtr = Skel.FindBone(BoneIDParam);
        if (!BonePtr) return;

        BoneID NewID(NextBoneID++);
        IDMap[BoneIDParam] = NewID;

        Bone NewBone(NewID, BonePtr->Name, NewParentID, BonePtr->LocalTransform, BonePtr->InverseBindMatrix);
        MergedBones.push_back(NewBone);

        std::vector<BoneID> Children = Skel.GetChildren(BoneIDParam);
        for (BoneID ChildID : Children) {
            AddBoneRecursive(Skel, ChildID, NewID, IDMap);
        }
    };

    if (ARoot.IsValid()) {
        AddBoneRecursive(A, ARoot, NewRoot, AMap);
    }

    BoneID BRoot = B.GetRoot();
    std::map<BoneID, BoneID> BMap;

    if (BRoot.IsValid()) {
        AddBoneRecursive(B, BRoot, NewRoot, BMap);
    }

    BoneMap.insert(AMap.begin(), AMap.end());
    BoneMap.insert(BMap.begin(), BMap.end());

    return SkeletonTree(MergedBones, NewRoot);
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

        SkeletonTree MergedSkel = MergeSkeletons(RiggedA.MSkeleton, RiggedB.MSkeleton, "MergedRoot");

        std::map<BoneID, BoneID> BoneMapA, BoneMapB;

        Generator<MeshData> MeshGenA = Generator<MeshData>::Constant(RiggedA.Mesh);
        Generator<MeshData> MeshGenB = Generator<MeshData>::Constant(RiggedB.Mesh);
        MeshData MergedMesh = Merge(MeshGenA, MeshGenB)(SeedParam.Derive(2));

        SkinWeights MergedWeights(MergedMesh.GetVertexCount());

        for (size_t I = 0; I < RiggedA.MSkinWeights.GetVertexCount(); ++I) {
            MergedWeights.SetWeights(I, RiggedA.MSkinWeights.GetWeights(I));
        }

        uint32_t VertexOffset = static_cast<uint32_t>(RiggedA.Mesh.GetVertexCount());
        for (size_t I = 0; I < RiggedB.MSkinWeights.GetVertexCount(); ++I) {
            MergedWeights.SetWeights(I + VertexOffset, RiggedB.MSkinWeights.GetWeights(I));
        }

        return RiggedMesh(MergedMesh, MergedSkel, MergedWeights);
    }));
}

} // namespace Solstice::Arzachel
