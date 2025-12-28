#include "GLTFExport.hxx"
#include <Core/AssetLoader.hxx>
#include <tiny_gltf.h>
#include <fstream>
#include <sstream>

namespace Solstice::Arzachel {

void ExportMeshToGLTF(const Generator<MeshData>& Mesh, const std::filesystem::path& Path, const Seed& SeedParam, const ExportProfile& Profile) {
    // Realize the generator
    MeshData MeshDataResult = Mesh(SeedParam);

    // Create glTF model
    tinygltf::Model GLTFModel;
    tinygltf::Scene GLTFScene;
    GLTFScene.name = "Exported Scene";

    // Create a buffer for vertex data
    tinygltf::Buffer GLTFBuffer;
    std::vector<unsigned char> GLTFBufferData;

    // Add positions
    size_t PositionOffset = GLTFBufferData.size();
    for (const auto& Position : MeshDataResult.Positions) {
        const float* Data = reinterpret_cast<const float*>(&Position);
        GLTFBufferData.insert(GLTFBufferData.end(), reinterpret_cast<const unsigned char*>(Data), reinterpret_cast<const unsigned char*>(Data) + 3 * sizeof(float));
    }

    // Add normals
    size_t NormalOffset = GLTFBufferData.size();
    for (const auto& Normal : MeshDataResult.Normals) {
        const float* Data = reinterpret_cast<const float*>(&Normal);
        GLTFBufferData.insert(GLTFBufferData.end(), reinterpret_cast<const unsigned char*>(Data), reinterpret_cast<const unsigned char*>(Data) + 3 * sizeof(float));
    }

    // Add UVs
    size_t UVOffset = GLTFBufferData.size();
    for (const auto& UVValue : MeshDataResult.UVs) {
        const float* Data = reinterpret_cast<const float*>(&UVValue);
        GLTFBufferData.insert(GLTFBufferData.end(), reinterpret_cast<const unsigned char*>(Data), reinterpret_cast<const unsigned char*>(Data) + 2 * sizeof(float));
    }

    // Add indices
    size_t IndexOffset = GLTFBufferData.size();
    for (uint32_t Index : MeshDataResult.Indices) {
        const uint32_t* Data = &Index;
        GLTFBufferData.insert(GLTFBufferData.end(), reinterpret_cast<const unsigned char*>(Data), reinterpret_cast<const unsigned char*>(Data) + sizeof(uint32_t));
    }

    GLTFBuffer.data = GLTFBufferData;
    GLTFModel.buffers.push_back(GLTFBuffer);

    // Create buffer views
    tinygltf::BufferView PositionView;
    PositionView.buffer = 0;
    PositionView.byteOffset = static_cast<int>(PositionOffset);
    PositionView.byteLength = static_cast<int>(MeshDataResult.Positions.size() * 3 * sizeof(float));
    PositionView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    GLTFModel.bufferViews.push_back(PositionView);

    tinygltf::BufferView NormalView;
    NormalView.buffer = 0;
    NormalView.byteOffset = static_cast<int>(NormalOffset);
    NormalView.byteLength = static_cast<int>(MeshDataResult.Normals.size() * 3 * sizeof(float));
    NormalView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    GLTFModel.bufferViews.push_back(NormalView);

    tinygltf::BufferView UVView;
    UVView.buffer = 0;
    UVView.byteOffset = static_cast<int>(UVOffset);
    UVView.byteLength = static_cast<int>(MeshDataResult.UVs.size() * 2 * sizeof(float));
    UVView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    GLTFModel.bufferViews.push_back(UVView);

    tinygltf::BufferView IndexView;
    IndexView.buffer = 0;
    IndexView.byteOffset = static_cast<int>(IndexOffset);
    IndexView.byteLength = static_cast<int>(MeshDataResult.Indices.size() * sizeof(uint32_t));
    IndexView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    GLTFModel.bufferViews.push_back(IndexView);

    // Create accessors
    tinygltf::Accessor PositionAccessor;
    PositionAccessor.bufferView = 0;
    PositionAccessor.byteOffset = 0;
    PositionAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    PositionAccessor.count = static_cast<int>(MeshDataResult.Positions.size());
    PositionAccessor.type = TINYGLTF_TYPE_VEC3;
    PositionAccessor.minValues = {MeshDataResult.BoundsMin.x, MeshDataResult.BoundsMin.y, MeshDataResult.BoundsMin.z};
    PositionAccessor.maxValues = {MeshDataResult.BoundsMax.x, MeshDataResult.BoundsMax.y, MeshDataResult.BoundsMax.z};
    GLTFModel.accessors.push_back(PositionAccessor);

    tinygltf::Accessor NormalAccessor;
    NormalAccessor.bufferView = 1;
    NormalAccessor.byteOffset = 0;
    NormalAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    NormalAccessor.count = static_cast<int>(MeshDataResult.Normals.size());
    NormalAccessor.type = TINYGLTF_TYPE_VEC3;
    GLTFModel.accessors.push_back(NormalAccessor);

    tinygltf::Accessor UVAccessor;
    UVAccessor.bufferView = 2;
    UVAccessor.byteOffset = 0;
    UVAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    UVAccessor.count = static_cast<int>(MeshDataResult.UVs.size());
    UVAccessor.type = TINYGLTF_TYPE_VEC2;
    GLTFModel.accessors.push_back(UVAccessor);

    tinygltf::Accessor IndexAccessor;
    IndexAccessor.bufferView = 3;
    IndexAccessor.byteOffset = 0;
    IndexAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    IndexAccessor.count = static_cast<int>(MeshDataResult.Indices.size());
    IndexAccessor.type = TINYGLTF_TYPE_SCALAR;
    GLTFModel.accessors.push_back(IndexAccessor);

    // Create primitive
    tinygltf::Primitive GLTFPrimitive;
    GLTFPrimitive.attributes["POSITION"] = 0;
    GLTFPrimitive.attributes["NORMAL"] = 1;
    GLTFPrimitive.attributes["TEXCOORD_0"] = 2;
    GLTFPrimitive.indices = 3;
    GLTFPrimitive.mode = TINYGLTF_MODE_TRIANGLES;

    // Create mesh
    tinygltf::Mesh GLTFMesh;
    GLTFMesh.name = "ExportedMesh";
    GLTFMesh.primitives.push_back(GLTFPrimitive);
    GLTFModel.meshes.push_back(GLTFMesh);

    // Create node
    tinygltf::Node GLTFNode;
    GLTFNode.name = "MeshNode";
    GLTFNode.mesh = 0;
    GLTFModel.nodes.push_back(GLTFNode);

    // Create scene
    GLTFScene.nodes.push_back(0);
    GLTFModel.scenes.push_back(GLTFScene);
    GLTFModel.defaultScene = 0;

    // Write file
    tinygltf::TinyGLTF Writer;
    std::string ErrorStr, WarningStr;
    bool SuccessResult = false;

    if (Profile.BinaryFormat) {
        SuccessResult = Writer.WriteGltfSceneToFile(&GLTFModel, Path.string(), false, false, false, true);
    } else {
        SuccessResult = Writer.WriteGltfSceneToFile(&GLTFModel, Path.string(), false, false, true, false);
    }

    if (!SuccessResult) {
        // Log error if needed
    }
}

void ExportRiggedToGLTF(const Generator<RiggedMesh>& RiggedMeshGen, const std::filesystem::path& Path, const Seed& SeedParam, const ExportProfile& Profile) {
    // Realize the generator
    RiggedMesh RiggedMeshResult = RiggedMeshGen(SeedParam);

    // Export mesh first (simplified - would need to export skeleton and skin weights)
    Generator<MeshData> MeshGen = Generator<MeshData>::Constant(RiggedMeshResult.Mesh);
    ExportMeshToGLTF(MeshGen, Path, SeedParam, Profile);

    // TODO: Export skeleton and skin weights to glTF
    // This requires adding skin and joints to the glTF model
}

void ExportAnimationToGLTF(const Generator<AnimationClip>& ClipGen, const Skeleton& SkeletonParam, const std::filesystem::path& Path, const Seed& SeedParam, const ExportProfile& Profile) {
    // Realize the generator
    AnimationClip AnimationResult = ClipGen(SeedParam);

    // TODO: Export animation to glTF
    // This requires creating animation channels and samplers in glTF format
    // For now, create empty model
    tinygltf::Model GLTFModel;
    tinygltf::TinyGLTF Writer;
    Writer.WriteGltfSceneToFile(&GLTFModel, Path.string(), false, false, true, Profile.BinaryFormat);
}

} // namespace Solstice::Arzachel
