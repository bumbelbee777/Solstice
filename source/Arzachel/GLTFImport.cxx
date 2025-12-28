#include "GLTFImport.hxx"
#include <Core/AssetLoader.hxx>
#include <Render/Mesh.hxx>
#include <fstream>
#include <sstream>
#include <cstdint>

namespace Solstice::Arzachel {

// Simple hash function for file contents
uint64_t HashFileContents(const std::filesystem::path& Path) {
    std::ifstream File(Path, std::ios::binary);
    if (!File.is_open()) {
        return 0;
    }

    uint64_t HashResult = 0x9e3779b97f4a7c15ULL;
    char Buffer[4096];
    while (File.read(Buffer, sizeof(Buffer))) {
        for (size_t I = 0; I < File.gcount(); ++I) {
            HashResult ^= static_cast<uint64_t>(Buffer[I]);
            HashResult = (HashResult << 13) | (HashResult >> 51);
            HashResult = HashResult * 5 + 0xe6546b64ULL;
        }
    }
    for (size_t I = 0; I < static_cast<size_t>(File.gcount()); ++I) {
        HashResult ^= static_cast<uint64_t>(Buffer[I]);
        HashResult = (HashResult << 13) | (HashResult >> 51);
        HashResult = HashResult * 5 + 0xe6546b64ULL;
    }

    return HashResult;
}

Generator<MeshData> ImportMeshFromGLTF(const std::filesystem::path& Path, uint32_t MeshIndex) {
    return Generator<MeshData>([Path, MeshIndex](const Seed& SeedParam) {
        // Load glTF using Core::AssetLoader
        Core::AssetLoadResult LoadResult = Core::AssetLoader::LoadGLTF(Path);

        if (!LoadResult.Success || MeshIndex >= LoadResult.Meshes.size()) {
            return MeshData{}; // Return empty mesh on error
        }

        // Convert Render::Mesh to MeshData
        const Render::Mesh* RenderMeshPtr = LoadResult.Meshes[MeshIndex].get();
        if (!RenderMeshPtr) {
            return MeshData{};
        }

        MeshData MeshDataResult;
        MeshDataResult.Positions.reserve(RenderMeshPtr->Vertices.size());
        MeshDataResult.Normals.reserve(RenderMeshPtr->Vertices.size());
        MeshDataResult.UVs.reserve(RenderMeshPtr->Vertices.size());
        MeshDataResult.Indices = RenderMeshPtr->Indices;

        for (const auto& VertexObj : RenderMeshPtr->Vertices) {
            MeshDataResult.Positions.push_back(VertexObj.GetPosition(Math::Vec3{}, Math::Vec3{}));
            MeshDataResult.Normals.push_back(VertexObj.GetNormal());
            MeshDataResult.UVs.push_back(VertexObj.GetUV());
        }

        // Convert submeshes
        for (const auto& SubMeshObj : RenderMeshPtr->SubMeshes) {
            MeshDataResult.SubMeshes.emplace_back(SubMeshObj.MaterialID, SubMeshObj.IndexStart, SubMeshObj.IndexCount);
        }

        MeshDataResult.CalculateBounds();
        return MeshDataResult;
    });
}

Generator<RiggedMesh> ImportRiggedFromGLTF(const std::filesystem::path& Path, uint32_t MeshIndex) {
    return Generator<RiggedMesh>(std::function<RiggedMesh(const Seed&)>([Path, MeshIndex](const Seed& SeedParam) {
        // For now, import as mesh and add a simple root bone
        // TODO: Parse actual skeleton and skin weights from glTF
        Generator<MeshData> MeshGen = ImportMeshFromGLTF(Path, MeshIndex);
        MeshData MeshDataResult = MeshGen(SeedParam.Derive(0));

        // Create simple root bone skeleton
        BoneID RootID(0);
        Bone RootBone(RootID, "Root", BoneID{}, Math::Matrix4::Identity(), Math::Matrix4::Identity());
        Skeleton SkeletonResult({RootBone}, RootID);

        // Assign all vertices to root
        SkinWeights WeightsResult(MeshDataResult.GetVertexCount());
        for (size_t I = 0; I < MeshDataResult.GetVertexCount(); ++I) {
            VertexWeights VW;
            VW.Influences[0] = SkinWeight(RootID, 1.0f);
            WeightsResult.SetWeights(I, VW);
        }

        return RiggedMesh(MeshDataResult, SkeletonResult, WeightsResult);
    }));
}

Generator<AnimationClip> ImportAnimationFromGLTF(const std::filesystem::path& Path, uint32_t AnimIndex) {
    return Generator<AnimationClip>([Path, AnimIndex](const Seed& SeedParam) {
        // TODO: Parse animation from glTF
        // For now, return empty clip
        // This requires parsing glTF animation channels and samplers
        return AnimationClip{};
    });
}

} // namespace Solstice::Arzachel
