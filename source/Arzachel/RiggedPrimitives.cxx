#include "RiggedPrimitives.hxx"
#include "Polyhedra.hxx"
#include <Math/Matrix.hxx>
#include <cmath>

namespace Solstice::Arzachel {

Generator<RiggedMesh> RigidRoot(const Generator<MeshData>& Mesh, const Seed& SeedParam) {
    return Generator<RiggedMesh>(std::function<RiggedMesh(const Seed&)>([Mesh](const Seed& SeedParamInner) {
        MeshData MeshDataResult = Mesh(SeedParamInner.Derive(0));

        // Create single root bone
        BoneID RootID(0);
        Bone RootBone(RootID, "Root", BoneID{}, Math::Matrix4::Identity(), Math::Matrix4::Identity());

        std::vector<Bone> BonesVec = {RootBone};
        Skeleton SkeletonResult(BonesVec, RootID);

        // Assign all vertices to root bone with weight 1.0
        SkinWeights WeightsResult(MeshDataResult.GetVertexCount());
        for (size_t I = 0; I < MeshDataResult.GetVertexCount(); ++I) {
            VertexWeights VWResult;
            VWResult.Influences[0] = SkinWeight(RootID, 1.0f);
            WeightsResult.SetWeights(I, VWResult);
        }

        return RiggedMesh(MeshDataResult, SkeletonResult, WeightsResult);
    }));
}

Generator<RiggedMesh> Chain(const Generator<MeshData>& Mesh, int BoneCount, const Seed& SeedParam) {
    return Generator<RiggedMesh>(std::function<RiggedMesh(const Seed&)>([Mesh, BoneCount](const Seed& SeedParamInner) mutable {
        MeshData MeshDataResult = Mesh(SeedParamInner.Derive(0));

        if (BoneCount < 1) BoneCount = 1;

        std::vector<Bone> BonesVec;
        BoneID RootID(0);
        Bone RootBone(RootID, "Root", BoneID{}, Math::Matrix4::Identity(), Math::Matrix4::Identity());
        BonesVec.push_back(RootBone);

        // Create chain of bones along Y axis
        float SegmentLength = 1.0f / static_cast<float>(BoneCount);
        for (int I = 1; I < BoneCount; ++I) {
            BoneID BoneIDObj(I);
            BoneID ParentID(I - 1);
            Math::Matrix4 LocalTransform = Math::Matrix4::Translation(Math::Vec3(0, SegmentLength, 0));
            BonesVec.emplace_back(BoneIDObj, "Bone" + std::to_string(I), ParentID, LocalTransform, Math::Matrix4::Identity());
        }

        Skeleton SkeletonResult(BonesVec, RootID);

        // Assign vertices to nearest bone (simplified - evenly distribute)
        SkinWeights WeightsResult(MeshDataResult.GetVertexCount());
        float YRange = MeshDataResult.BoundsMax.y - MeshDataResult.BoundsMin.y;

        for (size_t I = 0; I < MeshDataResult.GetVertexCount(); ++I) {
            float YVal = MeshDataResult.Positions[I].y;
            float NormalizedY = (YVal - MeshDataResult.BoundsMin.y) / (YRange > 0.0f ? YRange : 1.0f);

            int BoneIndex = static_cast<int>(NormalizedY * BoneCount);
            BoneIndex = std::max(0, std::min(BoneIndex, BoneCount - 1));

            VertexWeights VWResult;
            VWResult.Influences[0] = SkinWeight(BoneID(BoneIndex), 1.0f);
            WeightsResult.SetWeights(I, VWResult);
        }

        return RiggedMesh(MeshDataResult, SkeletonResult, WeightsResult);
    }));
}

Generator<RiggedMesh> Spine(const Generator<MeshData>& Mesh, int Segments, const Seed& SeedParam) {
    // Spine is similar to chain but with more sophisticated weight distribution
    return Chain(Mesh, Segments, SeedParam);
}

Generator<RiggedMesh> HumanoidRig(const Seed& SeedParam) {
    return Generator<RiggedMesh>([](const Seed& SeedParamInner) {
        MeshData FullMesh = Cube(1.0f, SeedParamInner)(SeedParamInner);
        BoneID RootID(0), SpineID(1);
        std::vector<Bone> Bones = {
            Bone(RootID, "Root", BoneID{}, Math::Matrix4::Identity(), Math::Matrix4::Identity()),
            Bone(SpineID, "Spine", RootID, Math::Matrix4::Translation(Math::Vec3(0, 0.5f, 0)), Math::Matrix4::Identity())
        };
        Skeleton Skel(Bones, RootID); SkinWeights Weights(FullMesh.GetVertexCount());
        RiggedMesh RM(FullMesh, Skel, Weights); AutoWeight(RM); return RM;
    });
}

Generator<RiggedMesh> AnimalRig(const Seed& SeedParam) { return HumanoidRig(SeedParam); }

void AutoWeight(RiggedMesh& Mesh) {
    for (size_t I = 0; I < Mesh.Mesh.GetVertexCount(); ++I) {
        Math::Vec3 P = Mesh.Mesh.Positions[I]; BoneID Best = Mesh.MSkeleton.GetRoot(); float MinD = 1e10f;
        for (const auto& B : Mesh.MSkeleton.GetBones()) {
            float D = (P - B.LocalTransform.GetTranslation()).Length();
            if (D < MinD) { MinD = D; Best = B.ID; }
        }
        VertexWeights VW; VW.Influences[0] = SkinWeight(Best, 1.0f); Mesh.MSkinWeights.SetWeights(I, VW);
    }
}

} // namespace Solstice::Arzachel
