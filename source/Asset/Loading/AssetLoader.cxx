#include <Asset/Loading/AssetLoader.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Material/Material.hxx>
#include <Core/Debug/Debug.hxx>
#include <Asset/IO/WavefrontParser.hxx>
#include <Core/Relic/Relic.hxx>
#include <Arzachel/AnimationClip.hxx>
#include <Arzachel/AnimationTrack.hxx>
#include <MinGfx/Keyframe.hxx>
#include <Skeleton/Skeleton.hxx>
#include <Arzachel/SkinWeights.hxx>
#include <Math/Quaternion.hxx>
#include <Math/Matrix.hxx>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <iostream>
#include <cstring>
#include <tuple>
#include <algorithm>
#include <unordered_map>

namespace Solstice::Core {

// --------------------------------------------------------------------------
//  Static member definitions
// --------------------------------------------------------------------------
std::filesystem::path AssetLoader::m_AssetPath = "./assets";
std::unordered_map<std::string, uint32_t> AssetLoader::m_LoadedAssets;

// --------------------------------------------------------------------------
//  Basic helpers
// --------------------------------------------------------------------------
void AssetLoader::SetAssetPath(const std::filesystem::path& Path) {
    m_AssetPath = Path;
    SOLSTICE_LOG("AssetLoader: Asset path set to: " + m_AssetPath.string());
}

const std::filesystem::path& AssetLoader::GetAssetPath() {
    return m_AssetPath;
}

std::filesystem::path AssetLoader::ResolvePath(const std::filesystem::path& Path) {
    if (Path.is_absolute())
        return Path;
    return m_AssetPath / Path;
}

// --------------------------------------------------------------------------
//  Load a glTF file – the heart of the loader
// --------------------------------------------------------------------------
AssetLoadResult AssetLoader::LoadGLTF(const std::filesystem::path& Path) {
    AssetLoadResult Result;
    tinygltf::Model Model;
    tinygltf::TinyGLTF Loader;
    std::string Error, Warning;
    bool LoadSuccess = false;

    if (Core::Relic::IsInitialized()) {
        auto* svc = Core::Relic::GetAssetService();
        if (svc) {
            std::string pathStr = Path.string();
            auto hashOpt = svc->PathToHash(pathStr);
            if (hashOpt) {
                auto bytesOpt = svc->LoadByHash(*hashOpt);
                if (bytesOpt && !bytesOpt->empty()) {
                    const std::byte* data = bytesOpt->data();
                    size_t size = bytesOpt->size();
                    if (size >= 4 && std::memcmp(data, "glTF", 4) == 0) {
                        LoadSuccess = Loader.LoadBinaryFromMemory(&Model, &Error, &Warning,
                            reinterpret_cast<const unsigned char*>(data), static_cast<unsigned int>(size), "");
                    } else {
                        LoadSuccess = Loader.LoadASCIIFromString(&Model, &Error, &Warning,
                            reinterpret_cast<const char*>(data), size, "");
                    }
                }
            }
        }
    }

    if (!LoadSuccess) {
        const std::filesystem::path FullPath = ResolvePath(Path);
        if (!std::filesystem::exists(FullPath)) {
            Result.ErrorMessage = "File not found: " + FullPath.string();
            SOLSTICE_LOG("AssetLoader: " + Result.ErrorMessage);
            return Result;
        }
        const std::string Extension = FullPath.extension().string();
        if (Extension == ".gltf")
            LoadSuccess = Loader.LoadASCIIFromFile(&Model, &Error, &Warning, FullPath.string());
        else if (Extension == ".glb")
            LoadSuccess = Loader.LoadBinaryFromFile(&Model, &Error, &Warning, FullPath.string());
        else {
            Result.ErrorMessage = "Unsupported file format: " + Extension;
            SOLSTICE_LOG("AssetLoader: " + Result.ErrorMessage);
            return Result;
        }
    }

    if (!Warning.empty())
        SOLSTICE_LOG("AssetLoader: " + Warning);

    if (!LoadSuccess) {
        Result.ErrorMessage = Error.empty() ? "Unknown error loading glTF file" : Error;
        SOLSTICE_LOG("AssetLoader: " + Result.ErrorMessage);
        return Result;
    }

    std::string sourceName = Path.string();
    SOLSTICE_LOG("AssetLoader: Successfully loaded glTF file: " + sourceName);
    SOLSTICE_LOG("  Meshes: " + std::to_string(Model.meshes.size()));
    SOLSTICE_LOG("  Materials: " + std::to_string(Model.materials.size()));
    SOLSTICE_LOG("  Textures: " + std::to_string(Model.textures.size()));
    SOLSTICE_LOG("  Nodes: " + std::to_string(Model.nodes.size()));

    // ----------------------------------------------------------------------
    //  Skins (must be loaded before meshes to get joint mappings)
    // ----------------------------------------------------------------------
    Result.Skeletons.reserve(Model.skins.size());
    std::vector<std::vector<int>> JointMappings; // Per-skin mapping from glTF joint index to BoneID
    JointMappings.reserve(Model.skins.size());

    for (size_t i = 0; i < Model.skins.size(); ++i) {
        auto skeleton = ConvertSkin(&Model, static_cast<int>(i));
        if (skeleton) {
            Result.Skeletons.push_back(std::move(skeleton));

            // Build joint mapping for this skin
            const tinygltf::Skin& skin = Model.skins[i];
            std::vector<int> jointMapping;
            jointMapping.reserve(skin.joints.size());
            for (size_t j = 0; j < skin.joints.size(); ++j) {
                jointMapping.push_back(static_cast<int>(j)); // Map glTF joint index to BoneID value
            }
            JointMappings.push_back(std::move(jointMapping));
        } else {
            JointMappings.push_back({});
        }
    }

    // ----------------------------------------------------------------------
    //  Meshes (with extended vertex data and skin weights)
    // ----------------------------------------------------------------------
    Result.Meshes.reserve(Model.meshes.size());
    Result.MeshExtendedData.reserve(Model.meshes.size());
    Result.MeshSkinWeights.reserve(Model.meshes.size());
    Result.MeshSkinIndices.reserve(Model.meshes.size());

    for (size_t i = 0; i < Model.meshes.size(); ++i) {
        // Find skin index for this mesh (check nodes)
        int skinIndex = -1;
        for (const auto& node : Model.nodes) {
            if (node.mesh == static_cast<int>(i) && node.skin >= 0) {
                skinIndex = node.skin;
                break;
            }
        }

        const std::vector<int>* jointMapping = nullptr;
        if (skinIndex >= 0 && skinIndex < static_cast<int>(JointMappings.size())) {
            jointMapping = &JointMappings[skinIndex];
        }

        auto tuple = ConvertMesh(&Model, static_cast<int>(i), jointMapping);
        if (std::get<0>(tuple)) {
            Result.Meshes.push_back(std::move(std::get<0>(tuple)));
            Result.MeshExtendedData.push_back(std::move(std::get<1>(tuple)));
            if (std::get<2>(tuple)) {
                Result.MeshSkinWeights.push_back(std::move(std::get<2>(tuple)));
            } else {
                Result.MeshSkinWeights.push_back(nullptr);
            }
            Result.MeshSkinIndices.push_back(skinIndex);
        } else {
            Result.MeshSkinWeights.push_back(nullptr);
            Result.MeshSkinIndices.push_back(-1);
        }
    }

    // ----------------------------------------------------------------------
    //  Materials
    // ----------------------------------------------------------------------
    Result.Materials.reserve(Model.materials.size());
    for (size_t i = 0; i < Model.materials.size(); ++i)
        Result.Materials.push_back(ConvertMaterial(&Model, static_cast<int>(i)));

    // ----------------------------------------------------------------------
    //  Cameras
    // ----------------------------------------------------------------------
    Result.Cameras.reserve(Model.cameras.size());
    for (size_t i = 0; i < Model.cameras.size(); ++i)
        Result.Cameras.push_back(ConvertCamera(&Model, static_cast<int>(i)));

    // ----------------------------------------------------------------------
    //  Lights (KHR_lights_punctual)
    // ----------------------------------------------------------------------
    if (Model.extensions.find("KHR_lights_punctual") != Model.extensions.end()) {
        const auto& lightsExt = Model.extensions.at("KHR_lights_punctual");
        if (lightsExt.Has("lights")) {
            const auto& lights = lightsExt.Get("lights");
            if (lights.IsArray()) {
                Result.Lights.reserve(lights.ArrayLen());
                for (size_t i = 0; i < lights.ArrayLen(); ++i)
                    Result.Lights.push_back(ConvertLight(&Model, static_cast<int>(i)));
            }
        }
    }

    // ----------------------------------------------------------------------
    //  Texture paths
    // ----------------------------------------------------------------------
    Result.TexturePaths.reserve(Model.images.size());
    for (const auto& Image : Model.images)
        Result.TexturePaths.push_back(Image.uri);

    // ----------------------------------------------------------------------
    //  Scene hierarchy (nodes)
    // ----------------------------------------------------------------------
    Result.Nodes.reserve(Model.nodes.size());
    for (const auto& GLTFNode : Model.nodes) {
        AssetLoadResult::Node Node;
        Node.Name        = GLTFNode.name;
        Node.MeshIndex   = GLTFNode.mesh;
        Node.CameraIndex = GLTFNode.camera;
        Node.LightIndex  = -1; // default – will be overwritten if present

        // Light index (extension)
        if (GLTFNode.extensions.find("KHR_lights_punctual") != GLTFNode.extensions.end()) {
            const auto& ext = GLTFNode.extensions.at("KHR_lights_punctual");
            if (ext.Has("light"))
                Node.LightIndex = ext.Get("light").Get<int>();
        }

        // Identity transform
        std::memset(Node.Transform, 0, sizeof(Node.Transform));
        Node.Transform[0] = Node.Transform[5] = Node.Transform[10] = Node.Transform[15] = 1.0f;

        // Matrix (if supplied)
        if (GLTFNode.matrix.size() == 16) {
            for (int i = 0; i < 16; ++i)
                Node.Transform[i] = static_cast<float>(GLTFNode.matrix[i]);
        }
        // TODO: TRS → matrix conversion could be added here.

        Node.Children.reserve(GLTFNode.children.size());
        for (int child : GLTFNode.children)
            Node.Children.push_back(child);

        Result.Nodes.push_back(std::move(Node));
    }

    // Root nodes from default scene
    if (Model.defaultScene >= 0 && Model.defaultScene < static_cast<int>(Model.scenes.size()))
        Result.RootNodes = Model.scenes[Model.defaultScene].nodes;
    else if (!Model.scenes.empty())
        Result.RootNodes = Model.scenes[0].nodes;

    // ----------------------------------------------------------------------
    //  Animations
    // ----------------------------------------------------------------------
    Result.Animations.reserve(Model.animations.size());
    for (size_t i = 0; i < Model.animations.size(); ++i) {
        auto anim = ConvertAnimation(&Model, static_cast<int>(i));
        if (anim) {
            Result.Animations.push_back(std::move(anim));
        }
    }

    SOLSTICE_LOG("AssetLoader: Loaded " + std::to_string(Result.Animations.size()) + " animations");
    SOLSTICE_LOG("AssetLoader: Loaded " + std::to_string(Result.Skeletons.size()) + " skeletons");

    Result.Success = true;
    return Result;
}

// --------------------------------------------------------------------------
//  Convenience loaders
// --------------------------------------------------------------------------
std::unique_ptr<::Solstice::Render::Mesh> AssetLoader::LoadMesh(const std::filesystem::path& Path) {
    AssetLoadResult result = LoadGLTF(Path);
    if (result.Success && !result.Meshes.empty())
        return std::move(result.Meshes[0]);
    return nullptr;
}

std::vector<Material> AssetLoader::LoadMaterials(const std::filesystem::path& Path) {
    const std::filesystem::path FullPath = ResolvePath(Path);
    const std::string Extension = FullPath.extension().string();

    AssetLoadResult result;
    if (Extension == ".gltf" || Extension == ".glb") {
        result = LoadGLTF(Path);
    } else if (Extension == ".obj") {
        result = LoadOBJ(Path);
    } else {
        SOLSTICE_LOG("AssetLoader: Unsupported file format: " + Extension);
        return {};
    }

    if (result.Success)
        return std::move(result.Materials);
    return {};
}

int AssetLoader::LoadGLTFIntoLibraries(const std::filesystem::path& Path,
                                      ::Solstice::Render::MeshLibrary& MeshLib,
                                      MaterialLibrary& MatLib) {
    const std::filesystem::path FullPath = ResolvePath(Path);
    const std::string Extension = FullPath.extension().string();

    AssetLoadResult result;
    if (Extension == ".gltf" || Extension == ".glb") {
        result = LoadGLTF(Path);
    } else if (Extension == ".obj") {
        result = LoadOBJ(Path);
    } else {
        SOLSTICE_LOG("AssetLoader: Unsupported file format: " + Extension);
        return -1;
    }

    if (!result.Success)
        return -1;

    // Materials first
    for (auto& mat : result.Materials)
        MatLib.AddMaterial(mat);

    // Meshes
    int count = 0;
    for (auto& meshPtr : result.Meshes) {
        if (meshPtr) {
            MeshLib.AddMesh(std::move(meshPtr));
            ++count;
        }
    }
    return count;
}

void AssetLoader::ClearCache() {
    m_LoadedAssets.clear();
    SOLSTICE_LOG("AssetLoader: Cache cleared");
}

// --------------------------------------------------------------------------
//  Mesh conversion (returns mesh + extended vertex data + skin weights)
// --------------------------------------------------------------------------
std::tuple<std::unique_ptr<::Solstice::Render::Mesh>, ExtendedVertexData, std::unique_ptr<::Solstice::Arzachel::SkinWeights>>
AssetLoader::ConvertMesh(const void* GLTFModelPtr, int MeshIndex, const std::vector<int>* JointMapping) {
    const tinygltf::Model* Model = static_cast<const tinygltf::Model*>(GLTFModelPtr);

    if (MeshIndex < 0 || MeshIndex >= static_cast<int>(Model->meshes.size())) {
        SOLSTICE_LOG("AssetLoader: Invalid mesh index: " + std::to_string(MeshIndex));
        return {nullptr, ExtendedVertexData{}, nullptr};
    }

    const tinygltf::Mesh& GLTFMesh = Model->meshes[MeshIndex];
    auto MeshPtr = std::make_unique<::Solstice::Render::Mesh>();
    ExtendedVertexData ExtData;
    auto SkinWeightsPtr = std::make_unique<::Solstice::Arzachel::SkinWeights>();

    SOLSTICE_LOG("AssetLoader: Converting mesh '" + GLTFMesh.name + "' with " +
                 std::to_string(GLTFMesh.primitives.size()) + " primitives");

    uint32_t IndexOffset = 0;
    bool hasSkinWeights = false;
    for (const auto& Primitive : GLTFMesh.primitives) {
        bool primitiveHasWeights = ExtractPrimitiveData(Model, &Primitive, *MeshPtr, ExtData, SkinWeightsPtr.get(), JointMapping, IndexOffset);
        hasSkinWeights = hasSkinWeights || primitiveHasWeights;
    }

    MeshPtr->CalculateBounds();

    SOLSTICE_LOG("AssetLoader: Mesh converted - Vertices: " +
                 std::to_string(MeshPtr->GetVertexCount()) + ", Triangles: " +
                 std::to_string(MeshPtr->GetTriangleCount()));

    if (hasSkinWeights && SkinWeightsPtr->GetVertexCount() > 0) {
        return {std::move(MeshPtr), std::move(ExtData), std::move(SkinWeightsPtr)};
    } else {
        return {std::move(MeshPtr), std::move(ExtData), nullptr};
    }
}

// --------------------------------------------------------------------------
//  Primitive extraction – fills mesh and extended data
// --------------------------------------------------------------------------
bool AssetLoader::ExtractPrimitiveData(const void* GLTFModelPtr,
                                      const void* GLTFPrimitivePtr,
                                      ::Solstice::Render::Mesh& OutMesh,
                                      ExtendedVertexData& OutExtendedData,
                                      ::Solstice::Arzachel::SkinWeights* OutSkinWeights,
                                      const std::vector<int>* JointMapping,
                                      uint32_t& IndexOffset) {
    const tinygltf::Model* Model     = static_cast<const tinygltf::Model*>(GLTFModelPtr);
    const tinygltf::Primitive* Prim = static_cast<const tinygltf::Primitive*>(GLTFPrimitivePtr);

    // ------------------- POSITION (mandatory) -------------------
    auto posIt = Prim->attributes.find("POSITION");
    if (posIt == Prim->attributes.end()) {
        SOLSTICE_LOG("AssetLoader: Primitive has no POSITION attribute");
        return false;
    }
    const tinygltf::Accessor& posAcc = Model->accessors[posIt->second];
    const tinygltf::BufferView& posBV = Model->bufferViews[posAcc.bufferView];
    const tinygltf::Buffer& posBuf    = Model->buffers[posBV.buffer];
    const float* Positions = reinterpret_cast<const float*>(
        &posBuf.data[posBV.byteOffset + posAcc.byteOffset]);
    const size_t VertexCount = posAcc.count;

    // ------------------- NORMAL (optional) -------------------
    const float* Normals = nullptr;
    auto normIt = Prim->attributes.find("NORMAL");
    if (normIt != Prim->attributes.end()) {
        const tinygltf::Accessor& normAcc = Model->accessors[normIt->second];
        const tinygltf::BufferView& normBV = Model->bufferViews[normAcc.bufferView];
        const tinygltf::Buffer& normBuf    = Model->buffers[normBV.buffer];
        Normals = reinterpret_cast<const float*>(
            &normBuf.data[normBV.byteOffset + normAcc.byteOffset]);
    }

    // ------------------- TEXCOORD_0 (optional) -------------------
    const float* UVs = nullptr;
    auto uvIt = Prim->attributes.find("TEXCOORD_0");
    if (uvIt != Prim->attributes.end()) {
        const tinygltf::Accessor& uvAcc = Model->accessors[uvIt->second];
        const tinygltf::BufferView& uvBV = Model->bufferViews[uvAcc.bufferView];
        const tinygltf::Buffer& uvBuf    = Model->buffers[uvBV.buffer];
        UVs = reinterpret_cast<const float*>(
            &uvBuf.data[uvBV.byteOffset + uvAcc.byteOffset]);
    }

    // ------------------- COLOR_0 (optional) -------------------
    const void* Colors = nullptr;
    int ColorComponentType = 0;
    int ColorType = 0;
    auto colIt = Prim->attributes.find("COLOR_0");
    if (colIt != Prim->attributes.end()) {
        const tinygltf::Accessor& colAcc = Model->accessors[colIt->second];
        const tinygltf::BufferView& colBV = Model->bufferViews[colAcc.bufferView];
        const tinygltf::Buffer& colBuf    = Model->buffers[colBV.buffer];
        Colors = &colBuf.data[colBV.byteOffset + colAcc.byteOffset];
        ColorComponentType = colAcc.componentType;
        ColorType = colAcc.type;
        OutExtendedData.HasVertexColors = true;
    }

    // ------------------- TANGENT (optional) -------------------
    const float* Tangents = nullptr;
    auto tanIt = Prim->attributes.find("TANGENT");
    if (tanIt != Prim->attributes.end()) {
        const tinygltf::Accessor& tanAcc = Model->accessors[tanIt->second];
        const tinygltf::BufferView& tanBV = Model->bufferViews[tanAcc.bufferView];
        const tinygltf::Buffer& tanBuf    = Model->buffers[tanBV.buffer];
        Tangents = reinterpret_cast<const float*>(
            &tanBuf.data[tanBV.byteOffset + tanAcc.byteOffset]);
        OutExtendedData.HasTangents = true;
    }

    // ------------------- JOINTS_0 and WEIGHTS_0 (optional, for skinning) -------------------
    const void* Joints = nullptr;
    int JointComponentType = 0;
    const float* Weights = nullptr;
    bool hasSkinning = false;

    auto jointIt = Prim->attributes.find("JOINTS_0");
    auto weightIt = Prim->attributes.find("WEIGHTS_0");

    if (jointIt != Prim->attributes.end() && weightIt != Prim->attributes.end() && OutSkinWeights && JointMapping) {
        const tinygltf::Accessor& jointAcc = Model->accessors[jointIt->second];
        const tinygltf::BufferView& jointBV = Model->bufferViews[jointAcc.bufferView];
        const tinygltf::Buffer& jointBuf = Model->buffers[jointBV.buffer];
        Joints = &jointBuf.data[jointBV.byteOffset + jointAcc.byteOffset];
        JointComponentType = jointAcc.componentType;

        const tinygltf::Accessor& weightAcc = Model->accessors[weightIt->second];
        const tinygltf::BufferView& weightBV = Model->bufferViews[weightAcc.bufferView];
        const tinygltf::Buffer& weightBuf = Model->buffers[weightBV.buffer];
        Weights = reinterpret_cast<const float*>(
            &weightBuf.data[weightBV.byteOffset + weightAcc.byteOffset]);

        hasSkinning = true;
        // Initialize skin weights structure for the expected vertex count
        if (OutSkinWeights->GetVertexCount() < OutMesh.GetVertexCount() + VertexCount) {
            OutSkinWeights->SetWeights(OutMesh.GetVertexCount() + VertexCount - 1, ::Solstice::Arzachel::VertexWeights{});
        }
    }

    // ------------------- BUILD VERTICES -------------------
    uint32_t StartVertex = static_cast<uint32_t>(OutMesh.Vertices.size());
    for (size_t i = 0; i < VertexCount; ++i) {
        Math::Vec3 Pos(Positions[i * 3 + 0],
                      Positions[i * 3 + 1],
                      Positions[i * 3 + 2]);

        Math::Vec3 Normal(0.0f, 1.0f, 0.0f);
        if (Normals)
            Normal = Math::Vec3(Normals[i * 3 + 0],
                               Normals[i * 3 + 1],
                               Normals[i * 3 + 2]);

        Math::Vec2 UV(0.0f, 0.0f);
        if (UVs)
            UV = Math::Vec2(UVs[i * 2 + 0], UVs[i * 2 + 1]);

        OutMesh.AddVertex(Pos, Normal, UV);

        // Vertex colors
        if (Colors) {
            Math::Vec4 Color(1.0f, 1.0f, 1.0f, 1.0f);
            if (ColorComponentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                const float* fcol = static_cast<const float*>(Colors);
                if (ColorType == TINYGLTF_TYPE_VEC3)
                    Color = Math::Vec4(fcol[i * 3 + 0], fcol[i * 3 + 1], fcol[i * 3 + 2], 1.0f);
                else if (ColorType == TINYGLTF_TYPE_VEC4)
                    Color = Math::Vec4(fcol[i * 4 + 0], fcol[i * 4 + 1], fcol[i * 4 + 2], fcol[i * 4 + 3]);
            } else if (ColorComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* uscol = static_cast<const uint16_t*>(Colors);
                const float scale = 1.0f / 65535.0f;
                if (ColorType == TINYGLTF_TYPE_VEC3)
                    Color = Math::Vec4(uscol[i * 3 + 0] * scale,
                                      uscol[i * 3 + 1] * scale,
                                      uscol[i * 3 + 2] * scale, 1.0f);
                else if (ColorType == TINYGLTF_TYPE_VEC4)
                    Color = Math::Vec4(uscol[i * 4 + 0] * scale,
                                      uscol[i * 4 + 1] * scale,
                                      uscol[i * 4 + 2] * scale,
                                      uscol[i * 4 + 3] * scale);
            } else if (ColorComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                const uint8_t* ubcol = static_cast<const uint8_t*>(Colors);
                const float scale = 1.0f / 255.0f;
                if (ColorType == TINYGLTF_TYPE_VEC3)
                    Color = Math::Vec4(ubcol[i * 3 + 0] * scale,
                                      ubcol[i * 3 + 1] * scale,
                                      ubcol[i * 3 + 2] * scale, 1.0f);
                else if (ColorType == TINYGLTF_TYPE_VEC4)
                    Color = Math::Vec4(ubcol[i * 4 + 0] * scale,
                                      ubcol[i * 4 + 1] * scale,
                                      ubcol[i * 4 + 2] * scale,
                                      ubcol[i * 4 + 3] * scale);
            }
            OutExtendedData.VertexColors.push_back(Color);
        }

        // Tangents
        if (Tangents) {
            OutExtendedData.Tangents.push_back(
                Math::Vec4(Tangents[i * 4 + 0],
                           Tangents[i * 4 + 1],
                           Tangents[i * 4 + 2],
                           Tangents[i * 4 + 3]));
        }
    }

    // ------------------- INDICES -------------------
    if (Prim->indices >= 0) {
        const tinygltf::Accessor& idxAcc = Model->accessors[Prim->indices];
        const tinygltf::BufferView& idxBV = Model->bufferViews[idxAcc.bufferView];
        const tinygltf::Buffer& idxBuf    = Model->buffers[idxBV.buffer];
        const unsigned char* idxData = &idxBuf.data[idxBV.byteOffset + idxAcc.byteOffset];

        uint32_t StartIndex = static_cast<uint32_t>(OutMesh.Indices.size());

        for (size_t i = 0; i < idxAcc.count; ++i) {
            uint32_t Index = 0;
            switch (idxAcc.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                    const uint16_t* inds = reinterpret_cast<const uint16_t*>(idxData);
                    Index = inds[i];
                    break;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                    const uint32_t* inds = reinterpret_cast<const uint32_t*>(idxData);
                    Index = inds[i];
                    break;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                    Index = idxData[i];
                    break;
                }
                default:
                    SOLSTICE_LOG("AssetLoader: Unsupported index component type");
                    break;
            }
            OutMesh.Indices.push_back(StartVertex + Index);
        }

        uint32_t IndexCount = static_cast<uint32_t>(idxAcc.count);
        OutMesh.AddSubMesh(Prim->material, StartIndex, IndexCount);
    } else {
        // Generate a simple sequential index buffer
        uint32_t StartIndex = static_cast<uint32_t>(OutMesh.Indices.size());
        for (size_t i = 0; i < VertexCount; ++i)
            OutMesh.Indices.push_back(StartVertex + static_cast<uint32_t>(i));

        uint32_t IndexCount = static_cast<uint32_t>(VertexCount);
        OutMesh.AddSubMesh(Prim->material, StartIndex, IndexCount);
    }

    IndexOffset = static_cast<uint32_t>(OutMesh.Indices.size());
    return hasSkinning;
}

// --------------------------------------------------------------------------
//  Material conversion
// --------------------------------------------------------------------------
Material AssetLoader::ConvertMaterial(const void* GLTFModelPtr, int MaterialIndex) {
    const tinygltf::Model* Model = static_cast<const tinygltf::Model*>(GLTFModelPtr);
    Material Mat;

    if (MaterialIndex < 0 || MaterialIndex >= static_cast<int>(Model->materials.size())) {
        Mat.SetAlbedoColor(Math::Vec3(0.8f, 0.8f, 0.8f), 0.5f);
        return Mat;
    }

    const tinygltf::Material& GLTFMat = Model->materials[MaterialIndex];
    const auto& PBR = GLTFMat.pbrMetallicRoughness;

    // Base colour + roughness
    Math::Vec3 BaseColor(PBR.baseColorFactor[0],
                         PBR.baseColorFactor[1],
                         PBR.baseColorFactor[2]);
    float Roughness = static_cast<float>(PBR.roughnessFactor);
    Mat.SetAlbedoColor(BaseColor, Roughness);

    // Metallic factor (store in the packed field)
    Mat.Metallic = static_cast<uint8_t>(PBR.metallicFactor * 255.0f);

    // Emissive
    if (!GLTFMat.emissiveFactor.empty()) {
        Math::Vec3 Emissive(GLTFMat.emissiveFactor[0],
                            GLTFMat.emissiveFactor[1],
                            GLTFMat.emissiveFactor[2]);
        float EmissiveStrength = std::max({Emissive.x, Emissive.y, Emissive.z});
        Mat.SetEmission(Emissive, EmissiveStrength);
    }

    // Texture indices
    if (PBR.baseColorTexture.index >= 0)
        Mat.AlbedoTexIndex = static_cast<uint16_t>(PBR.baseColorTexture.index);
    if (GLTFMat.normalTexture.index >= 0)
        Mat.NormalMapIndex = static_cast<uint16_t>(GLTFMat.normalTexture.index);

    // Flags
    if (GLTFMat.doubleSided)
        Mat.Flags |= MaterialFlag_DoubleSided;

    // Alpha mode – store in the material's AlphaMode field
    if (GLTFMat.alphaMode == "BLEND")
        Mat.AlphaMode = static_cast<uint8_t>(AlphaMode::Blend);
    else if (GLTFMat.alphaMode == "MASK")
        Mat.AlphaMode = static_cast<uint8_t>(AlphaMode::Masked);
    else
        Mat.AlphaMode = static_cast<uint8_t>(AlphaMode::Opaque);

    SOLSTICE_LOG("AssetLoader: Converted material '" + GLTFMat.name + "'");
    return Mat;
}

// --------------------------------------------------------------------------
//  Camera conversion (already present – kept unchanged)
// --------------------------------------------------------------------------
CameraData AssetLoader::ConvertCamera(const void* GLTFModelPtr, int CameraIndex) {
    const tinygltf::Model* Model = static_cast<const tinygltf::Model*>(GLTFModelPtr);
    CameraData CamData;
    if (CameraIndex < 0 || CameraIndex >= static_cast<int>(Model->cameras.size()))
        return CamData;

    const auto& Cam = Model->cameras[CameraIndex];
    CamData.Name = Cam.name;

    if (Cam.type == "perspective") {
        CamData.IsPerspective = true;
        CamData.AspectRatio   = static_cast<float>(Cam.perspective.aspectRatio);
        CamData.Fov           = static_cast<float>(Cam.perspective.yfov) * 57.2958f;
        CamData.ZNear         = static_cast<float>(Cam.perspective.znear);
        CamData.ZFar          = static_cast<float>(Cam.perspective.zfar);
    } else if (Cam.type == "orthographic") {
        CamData.IsPerspective = false;
        CamData.XMag          = static_cast<float>(Cam.orthographic.xmag);
        CamData.YMag          = static_cast<float>(Cam.orthographic.ymag);
        CamData.ZNear         = static_cast<float>(Cam.orthographic.znear);
        CamData.ZFar          = static_cast<float>(Cam.orthographic.zfar);
    }
    return CamData;
}

// --------------------------------------------------------------------------
//  Light conversion (already present – kept unchanged)
// --------------------------------------------------------------------------
LightData AssetLoader::ConvertLight(const void* GLTFModelPtr, int LightIndex) {
    const tinygltf::Model* Model = static_cast<const tinygltf::Model*>(GLTFModelPtr);
    LightData Light;

    if (Model->extensions.find("KHR_lights_punctual") == Model->extensions.end())
        return Light;
    const auto& ext = Model->extensions.at("KHR_lights_punctual");
    if (!ext.Has("lights"))
        return Light;
    const auto& lights = ext.Get("lights");
    if (LightIndex < 0 || LightIndex >= lights.ArrayLen())
        return Light;

    const auto& l = lights.Get(LightIndex);

    if (l.Has("name"))
        Light.Name = l.Get("name").Get<std::string>();

    if (l.Has("color")) {
        const auto& c = l.Get("color");
        Light.Color = Math::Vec3(static_cast<float>(c.Get(0).Get<double>()),
                                 static_cast<float>(c.Get(1).Get<double>()),
                                 static_cast<float>(c.Get(2).Get<double>()));
    }

    if (l.Has("intensity"))
        Light.Intensity = static_cast<float>(l.Get("intensity").Get<double>());
    if (l.Has("range"))
        Light.Range = static_cast<float>(l.Get("range").Get<double>());

    if (l.Has("type")) {
        std::string type = l.Get("type").Get<std::string>();
        if (type == "point")
            Light.LightType = LightData::Type::Point;
        else if (type == "spot") {
            Light.LightType = LightData::Type::Spot;
            if (l.Has("spot")) {
                const auto& spot = l.Get("spot");
                if (spot.Has("innerConeAngle"))
                    Light.InnerConeAngle = static_cast<float>(spot.Get("innerConeAngle").Get<double>());
                if (spot.Has("outerConeAngle"))
                    Light.OuterConeAngle = static_cast<float>(spot.Get("outerConeAngle").Get<double>());
            }
        } else if (type == "directional")
            Light.LightType = LightData::Type::Directional;
    }

    return Light;
}

// --------------------------------------------------------------------------
//  Skin/Skeleton conversion
// --------------------------------------------------------------------------
std::unique_ptr<::Solstice::Skeleton::Skeleton> AssetLoader::ConvertSkin(const void* GLTFModelPtr, int SkinIndex) {
    const tinygltf::Model* Model = static_cast<const tinygltf::Model*>(GLTFModelPtr);

    if (SkinIndex < 0 || SkinIndex >= static_cast<int>(Model->skins.size())) {
        SOLSTICE_LOG("AssetLoader: Invalid skin index: " + std::to_string(SkinIndex));
        return nullptr;
    }

    const tinygltf::Skin& GLTFSkin = Model->skins[SkinIndex];

    // Extract inverse bind matrices
    std::vector<Math::Matrix4> InverseBindMatrices;
    if (GLTFSkin.inverseBindMatrices >= 0) {
        const tinygltf::Accessor& ibmAcc = Model->accessors[GLTFSkin.inverseBindMatrices];
        const tinygltf::BufferView& ibmBV = Model->bufferViews[ibmAcc.bufferView];
        const tinygltf::Buffer& ibmBuf = Model->buffers[ibmBV.buffer];
        const float* ibmData = reinterpret_cast<const float*>(
            &ibmBuf.data[ibmBV.byteOffset + ibmAcc.byteOffset]);

        InverseBindMatrices.reserve(ibmAcc.count);
        for (size_t i = 0; i < ibmAcc.count; ++i) {
            Math::Matrix4 mat;
            // glTF matrices are column-major
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                    mat.M[row][col] = ibmData[i * 16 + col * 4 + row];
                }
            }
            InverseBindMatrices.push_back(mat);
        }
    }

    // Build bone hierarchy from joint nodes
    std::vector<::Solstice::Skeleton::Bone> Bones;
    Bones.reserve(GLTFSkin.joints.size());

    // Map node indices to bone indices
    std::unordered_map<int, int> NodeToBoneIndex;
    for (size_t i = 0; i < GLTFSkin.joints.size(); ++i) {
        NodeToBoneIndex[GLTFSkin.joints[i]] = static_cast<int>(i);
    }

    // Find root bone (skeleton root or first joint without parent in joint list)
    ::Solstice::Skeleton::BoneID RootBoneID;
    int skeletonRootNode = GLTFSkin.skeleton >= 0 ? GLTFSkin.skeleton : -1;

    // Build bones
    for (size_t i = 0; i < GLTFSkin.joints.size(); ++i) {
        int nodeIndex = GLTFSkin.joints[i];
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(Model->nodes.size())) {
            SOLSTICE_LOG("AssetLoader: Invalid joint node index: " + std::to_string(nodeIndex));
            continue;
        }

        const tinygltf::Node& node = Model->nodes[nodeIndex];
        ::Solstice::Skeleton::BoneID boneId(static_cast<uint32_t>(i));
        ::Solstice::Skeleton::BoneID parentId;

        // Find parent bone (if parent is in joint list)
        if (!node.children.empty()) {
            // Check if any child is in the joint list - if so, this bone is a parent
            for (int childNodeIdx : node.children) {
                if (NodeToBoneIndex.find(childNodeIdx) != NodeToBoneIndex.end()) {
                    // This node has children in the skeleton, so it's a parent
                    break;
                }
            }
        }

        // Find parent in joint list by checking node hierarchy
        int parentBoneIndex = -1;
        for (size_t j = 0; j < Model->nodes.size(); ++j) {
            const auto& parentNode = Model->nodes[j];
            for (int childIdx : parentNode.children) {
                if (childIdx == nodeIndex) {
                    // Found parent node, check if it's in joint list
                    auto it = NodeToBoneIndex.find(static_cast<int>(j));
                    if (it != NodeToBoneIndex.end()) {
                        parentBoneIndex = it->second;
                        break;
                    }
                }
            }
            if (parentBoneIndex >= 0) break;
        }

        if (parentBoneIndex >= 0) {
            parentId = ::Solstice::Skeleton::BoneID(static_cast<uint32_t>(parentBoneIndex));
        }

        // Convert node transform to matrix
        Math::Matrix4 localTransform = Math::Matrix4::Identity();
        if (node.matrix.size() == 16) {
            // Direct matrix
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                    localTransform.M[row][col] = static_cast<float>(node.matrix[col * 4 + row]);
                }
            }
        } else {
            // TRS decomposition
            Math::Vec3 translation(0, 0, 0);
            Math::Quaternion rotation(1, 0, 0, 0);
            Math::Vec3 scale(1, 1, 1);

            if (node.translation.size() >= 3) {
                translation = Math::Vec3(
                    static_cast<float>(node.translation[0]),
                    static_cast<float>(node.translation[1]),
                    static_cast<float>(node.translation[2])
                );
            }

            if (node.rotation.size() >= 4) {
                // glTF quaternion is XYZW, our Quaternion is WXYZ
                rotation = Math::Quaternion(
                    static_cast<float>(node.rotation[3]), // w
                    static_cast<float>(node.rotation[0]), // x
                    static_cast<float>(node.rotation[1]), // y
                    static_cast<float>(node.rotation[2])  // z
                );
            }

            if (node.scale.size() >= 3) {
                scale = Math::Vec3(
                    static_cast<float>(node.scale[0]),
                    static_cast<float>(node.scale[1]),
                    static_cast<float>(node.scale[2])
                );
            }

            localTransform = Math::Matrix4::Translation(translation) *
                           rotation.ToMatrix() *
                           Math::Matrix4::Scale(scale);
        }

        // Get inverse bind matrix
        Math::Matrix4 inverseBind = Math::Matrix4::Identity();
        if (i < InverseBindMatrices.size()) {
            inverseBind = InverseBindMatrices[i];
        }

        std::string boneName = node.name.empty() ? ("Bone_" + std::to_string(i)) : node.name;
        Bones.emplace_back(boneId, boneName, parentId, localTransform, inverseBind);

        // Set root bone (first bone or skeleton root)
        if (i == 0 || (skeletonRootNode >= 0 && nodeIndex == skeletonRootNode)) {
            RootBoneID = boneId;
        }
    }

    if (Bones.empty()) {
        SOLSTICE_LOG("AssetLoader: No valid bones found in skin");
        return nullptr;
    }

    if (!RootBoneID.IsValid() && !Bones.empty()) {
        RootBoneID = Bones[0].ID;
    }

    auto skeleton = std::make_unique<::Solstice::Skeleton::Skeleton>(Bones, RootBoneID);
    SOLSTICE_LOG("AssetLoader: Converted skin '" + GLTFSkin.name + "' with " +
                 std::to_string(Bones.size()) + " bones");

    return skeleton;
}

// --------------------------------------------------------------------------
//  Animation conversion
// --------------------------------------------------------------------------
std::unique_ptr<::Solstice::Arzachel::AnimationClip> AssetLoader::ConvertAnimation(const void* GLTFModelPtr, int AnimationIndex) {
    const tinygltf::Model* Model = static_cast<const tinygltf::Model*>(GLTFModelPtr);

    if (AnimationIndex < 0 || AnimationIndex >= static_cast<int>(Model->animations.size())) {
        SOLSTICE_LOG("AssetLoader: Invalid animation index: " + std::to_string(AnimationIndex));
        return nullptr;
    }

    const tinygltf::Animation& GLTFAnim = Model->animations[AnimationIndex];
    auto clip = std::make_unique<::Solstice::Arzachel::AnimationClip>();

    // Group channels by target node to create tracks
    std::unordered_map<int, ::Solstice::Arzachel::AnimationTrack> nodeTracks;

    for (const auto& channel : GLTFAnim.channels) {
        if (channel.target_node < 0 || channel.target_node >= static_cast<int>(Model->nodes.size())) {
            continue;
        }

        const tinygltf::Node& targetNode = Model->nodes[channel.target_node];
        std::string nodeName = targetNode.name.empty() ? ("Node_" + std::to_string(channel.target_node)) : targetNode.name;

        // Get or create track for this node
        if (nodeTracks.find(channel.target_node) == nodeTracks.end()) {
            nodeTracks[channel.target_node] = ::Solstice::Arzachel::AnimationTrack(
                ::Solstice::Arzachel::BonePattern(nodeName)
            );
        }

        auto& track = nodeTracks[channel.target_node];

        // Get sampler
        if (channel.sampler < 0 || channel.sampler >= static_cast<int>(GLTFAnim.samplers.size())) {
            continue;
        }

        const tinygltf::AnimationSampler& sampler = GLTFAnim.samplers[channel.sampler];

        // Get input (time) accessor
        if (sampler.input < 0 || sampler.input >= static_cast<int>(Model->accessors.size())) {
            continue;
        }

        const tinygltf::Accessor& inputAcc = Model->accessors[sampler.input];
        const tinygltf::BufferView& inputBV = Model->bufferViews[inputAcc.bufferView];
        const tinygltf::Buffer& inputBuf = Model->buffers[inputBV.buffer];
        const float* times = reinterpret_cast<const float*>(
            &inputBuf.data[inputBV.byteOffset + inputAcc.byteOffset]);

        // Get output (value) accessor
        if (sampler.output < 0 || sampler.output >= static_cast<int>(Model->accessors.size())) {
            continue;
        }

        const tinygltf::Accessor& outputAcc = Model->accessors[sampler.output];
        const tinygltf::BufferView& outputBV = Model->bufferViews[outputAcc.bufferView];
        const tinygltf::Buffer& outputBuf = Model->buffers[outputBV.buffer];
        const void* values = &outputBuf.data[outputBV.byteOffset + outputAcc.byteOffset];

        // Map interpolation mode
        ::Solstice::MinGfx::InterpolationMode interpMode = ::Solstice::MinGfx::InterpolationMode::LINEAR;
        if (sampler.interpolation == "STEP") {
            interpMode = ::Solstice::MinGfx::InterpolationMode::STEP;
        } else if (sampler.interpolation == "CUBICSPLINE") {
            interpMode = ::Solstice::MinGfx::InterpolationMode::CUBIC;
        }

        // Convert based on target path
        std::string path = channel.target_path;

        if (path == "translation") {
            // Vec3 translation
            const float* vecData = static_cast<const float*>(values);
            for (size_t i = 0; i < inputAcc.count; ++i) {
                Math::Vec3 value(
                    vecData[i * 3 + 0],
                    vecData[i * 3 + 1],
                    vecData[i * 3 + 2]
                );
                track.Translation.AddKeyframe(::Solstice::MinGfx::Keyframe<Math::Vec3>(
                    times[i], value, interpMode
                ));
            }
        } else if (path == "rotation") {
            // Quaternion rotation (glTF is XYZW, we use WXYZ)
            const float* quatData = static_cast<const float*>(values);
            for (size_t i = 0; i < inputAcc.count; ++i) {
                Math::Quaternion value(
                    quatData[i * 4 + 3], // w
                    quatData[i * 4 + 0], // x
                    quatData[i * 4 + 1], // y
                    quatData[i * 4 + 2]  // z
                );
                track.Rotation.AddKeyframe(::Solstice::MinGfx::Keyframe<Math::Quaternion>(
                    times[i], value, interpMode
                ));
            }
        } else if (path == "scale") {
            // Vec3 scale
            const float* vecData = static_cast<const float*>(values);
            for (size_t i = 0; i < inputAcc.count; ++i) {
                Math::Vec3 value(
                    vecData[i * 3 + 0],
                    vecData[i * 3 + 1],
                    vecData[i * 3 + 2]
                );
                track.Scale.AddKeyframe(::Solstice::MinGfx::Keyframe<Math::Vec3>(
                    times[i], value, interpMode
                ));
            }
        }
    }

    // Add all tracks to clip
    for (auto& pair : nodeTracks) {
        clip->AddTrack(pair.second);
    }

    SOLSTICE_LOG("AssetLoader: Converted animation '" + GLTFAnim.name + "' with " +
                 std::to_string(nodeTracks.size()) + " tracks");

    return clip;
}

// --------------------------------------------------------------------------
//  OBJ file loading
// --------------------------------------------------------------------------
AssetLoadResult AssetLoader::LoadOBJ(const std::filesystem::path& Path) {
    AssetLoadResult Result;

    const std::filesystem::path FullPath = ResolvePath(Path);
    if (!std::filesystem::exists(FullPath)) {
        Result.ErrorMessage = "File not found: " + FullPath.string();
        SOLSTICE_LOG("AssetLoader: " + Result.ErrorMessage);
        return Result;
    }

    // Parse OBJ file
    WavefrontData objData = WavefrontParser::Parse(FullPath);
    if (!objData.Success) {
        Result.ErrorMessage = objData.Error.empty() ? "Failed to parse OBJ file" : objData.Error;
        SOLSTICE_LOG("AssetLoader: " + Result.ErrorMessage);
        return Result;
    }

    SOLSTICE_LOG("AssetLoader: Successfully loaded OBJ file: " + FullPath.string());
    SOLSTICE_LOG("  Vertices: " + std::to_string(objData.Positions.size()));
    SOLSTICE_LOG("  Triangles: " + std::to_string(objData.Indices.size() / 3));
    SOLSTICE_LOG("  Materials: " + std::to_string(objData.Materials.size()));

    // Convert materials
    Result.Materials.reserve(objData.Materials.size());
    for (const auto& objMat : objData.Materials) {
        Result.Materials.push_back(ConvertObjMaterial(objMat, FullPath));
    }

    // Create mesh from OBJ data
    auto meshPtr = std::make_unique<::Solstice::Render::Mesh>();
    ExtendedVertexData extData;

    // Add vertices
    for (size_t i = 0; i < objData.Positions.size(); ++i) {
        Math::Vec3 pos = objData.Positions[i];
        Math::Vec3 normal = (i < objData.Normals.size()) ? objData.Normals[i] : Math::Vec3(0, 1, 0);
        Math::Vec2 uv = (i < objData.UVs.size()) ? objData.UVs[i] : Math::Vec2(0, 0);
        meshPtr->AddVertex(pos, normal, uv);
    }

    // Add indices
    meshPtr->Indices = objData.Indices;

    // Create submeshes from OBJ submeshes
    for (const auto& objSubMesh : objData.SubMeshes) {
        // Find material index by name
        int materialIndex = -1;
        for (size_t i = 0; i < objData.Materials.size(); ++i) {
            if (objData.Materials[i].Name == objSubMesh.MaterialName) {
                materialIndex = static_cast<int>(i);
                break;
            }
        }
        if (materialIndex < 0) materialIndex = 0; // Default to first material

        meshPtr->AddSubMesh(materialIndex, objSubMesh.IndexStart, objSubMesh.IndexCount);
    }

    meshPtr->CalculateBounds();

    Result.Meshes.push_back(std::move(meshPtr));
    Result.MeshExtendedData.push_back(std::move(extData));
    Result.MeshSkinWeights.push_back(nullptr);
    Result.MeshSkinIndices.push_back(-1);

    // Extract texture paths from materials
    for (const auto& objMat : objData.Materials) {
        if (!objMat.DiffuseMap.empty()) {
            std::filesystem::path texPath = FullPath.parent_path() / objMat.DiffuseMap;
            Result.TexturePaths.push_back(texPath.string());
        }
        if (!objMat.NormalMap.empty()) {
            std::filesystem::path texPath = FullPath.parent_path() / objMat.NormalMap;
            Result.TexturePaths.push_back(texPath.string());
        }
    }

    Result.Success = true;
    return Result;
}

// --------------------------------------------------------------------------
//  OBJ material conversion
// --------------------------------------------------------------------------
Material AssetLoader::ConvertObjMaterial(const ObjMaterial& ObjMat, const std::filesystem::path& ObjFilePath) {
    Material Mat;

    // Map diffuse color to albedo
    // Convert shininess to roughness (inverse relationship: high shininess = low roughness)
    float roughness = 1.0f;
    if (ObjMat.Shininess > 0.0f) {
        // Map shininess (typically 0-1000) to roughness (0-1)
        // Higher shininess = lower roughness
        roughness = 1.0f - std::min(1.0f, ObjMat.Shininess / 1000.0f);
    }

    Mat.SetAlbedoColor(ObjMat.Diffuse, roughness);

    // Map specular to metallic (simplified - use specular intensity as metallic factor)
    float specularIntensity = (ObjMat.Specular.x + ObjMat.Specular.y + ObjMat.Specular.z) / 3.0f;
    Mat.Metallic = static_cast<uint8_t>(specularIntensity * 255.0f);

    // Map shininess to specular power
    Mat.SpecularPower = static_cast<uint8_t>(std::min(255.0f, ObjMat.Shininess / 4.0f));

    // Handle alpha
    if (ObjMat.Alpha < 1.0f) {
        Mat.AlphaMode = static_cast<uint8_t>(AlphaMode::Blend);
        Mat.Flags |= MaterialFlag_Transparent;
    } else {
        Mat.AlphaMode = static_cast<uint8_t>(AlphaMode::Opaque);
    }

    // Set material flags
    Mat.Flags |= MaterialFlag_CastsShadows | MaterialFlag_ReceivesShadows;
    if (!ObjMat.NormalMap.empty()) {
        Mat.Flags |= MaterialFlag_HasNormalMap;
    }

    // Texture indices would need to be resolved by looking up texture paths
    // For now, set to invalid (0xFFFF)
    Mat.AlbedoTexIndex = 0xFFFF;
    Mat.NormalMapIndex = 0xFFFF;

    // Use Blinn-Phong shading model for OBJ materials (legacy format)
    Mat.ShadingModel = static_cast<uint8_t>(ShadingModel::BlinnPhong);

    SOLSTICE_LOG("AssetLoader: Converted OBJ material '" + ObjMat.Name + "'");
    return Mat;
}

} // namespace Solstice::Core
