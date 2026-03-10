#include "AssetLoader.hxx"
#include <Render/Assets/Mesh.hxx>
#include <Core/Material.hxx>
#include <Core/Debug.hxx>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <iostream>
#include <cstring>

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

    const std::filesystem::path FullPath = ResolvePath(Path);
    if (!std::filesystem::exists(FullPath)) {
        Result.ErrorMessage = "File not found: " + FullPath.string();
        SOLSTICE_LOG("AssetLoader: " + Result.ErrorMessage);
        return Result;
    }

    tinygltf::Model Model;
    tinygltf::TinyGLTF Loader;
    std::string Error, Warning;
    bool LoadSuccess = false;
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

    if (!Warning.empty())
        SOLSTICE_LOG("AssetLoader: " + Warning);

    if (!LoadSuccess) {
        Result.ErrorMessage = Error.empty() ? "Unknown error loading glTF file" : Error;
        SOLSTICE_LOG("AssetLoader: " + Result.ErrorMessage);
        return Result;
    }

    SOLSTICE_LOG("AssetLoader: Successfully loaded glTF file: " + FullPath.string());
    SOLSTICE_LOG("  Meshes: " + std::to_string(Model.meshes.size()));
    SOLSTICE_LOG("  Materials: " + std::to_string(Model.materials.size()));
    SOLSTICE_LOG("  Textures: " + std::to_string(Model.textures.size()));
    SOLSTICE_LOG("  Nodes: " + std::to_string(Model.nodes.size()));

    // ----------------------------------------------------------------------
    //  Meshes (with extended vertex data)
    // ----------------------------------------------------------------------
    Result.Meshes.reserve(Model.meshes.size());
    Result.MeshExtendedData.reserve(Model.meshes.size());
    for (size_t i = 0; i < Model.meshes.size(); ++i) {
        auto pair = ConvertMesh(&Model, static_cast<int>(i));
        if (pair.first) {
            Result.Meshes.push_back(std::move(pair.first));
            Result.MeshExtendedData.push_back(std::move(pair.second));
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

    Result.Success = true;
    return Result;
}

// --------------------------------------------------------------------------
//  Convenience loaders
// --------------------------------------------------------------------------
std::unique_ptr<Render::Mesh> AssetLoader::LoadMesh(const std::filesystem::path& Path) {
    AssetLoadResult result = LoadGLTF(Path);
    if (result.Success && !result.Meshes.empty())
        return std::move(result.Meshes[0]);
    return nullptr;
}

std::vector<Material> AssetLoader::LoadMaterials(const std::filesystem::path& Path) {
    AssetLoadResult result = LoadGLTF(Path);
    if (result.Success)
        return std::move(result.Materials);
    return {};
}

int AssetLoader::LoadGLTFIntoLibraries(const std::filesystem::path& Path,
                                      Render::MeshLibrary& MeshLib,
                                      MaterialLibrary& MatLib) {
    AssetLoadResult result = LoadGLTF(Path);
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
//  Mesh conversion (returns mesh + extended vertex data)
// --------------------------------------------------------------------------
std::pair<std::unique_ptr<Render::Mesh>, ExtendedVertexData>
AssetLoader::ConvertMesh(const void* GLTFModelPtr, int MeshIndex) {
    const tinygltf::Model* Model = static_cast<const tinygltf::Model*>(GLTFModelPtr);

    if (MeshIndex < 0 || MeshIndex >= static_cast<int>(Model->meshes.size())) {
        SOLSTICE_LOG("AssetLoader: Invalid mesh index: " + std::to_string(MeshIndex));
        return {nullptr, ExtendedVertexData{}};
    }

    const tinygltf::Mesh& GLTFMesh = Model->meshes[MeshIndex];
    auto MeshPtr = std::make_unique<Render::Mesh>();
    ExtendedVertexData ExtData;

    SOLSTICE_LOG("AssetLoader: Converting mesh '" + GLTFMesh.name + "' with " +
                 std::to_string(GLTFMesh.primitives.size()) + " primitives");

    uint32_t IndexOffset = 0;
    for (const auto& Primitive : GLTFMesh.primitives)
        ExtractPrimitiveData(Model, &Primitive, *MeshPtr, ExtData, IndexOffset);

    MeshPtr->CalculateBounds();

    SOLSTICE_LOG("AssetLoader: Mesh converted - Vertices: " +
                 std::to_string(MeshPtr->GetVertexCount()) + ", Triangles: " +
                 std::to_string(MeshPtr->GetTriangleCount()));

    return {std::move(MeshPtr), std::move(ExtData)};
}

// --------------------------------------------------------------------------
//  Primitive extraction – fills mesh and extended data
// --------------------------------------------------------------------------
void AssetLoader::ExtractPrimitiveData(const void* GLTFModelPtr,
                                      const void* GLTFPrimitivePtr,
                                      Render::Mesh& OutMesh,
                                      ExtendedVertexData& OutExtendedData,
                                      uint32_t& IndexOffset) {
    const tinygltf::Model* Model     = static_cast<const tinygltf::Model*>(GLTFModelPtr);
    const tinygltf::Primitive* Prim = static_cast<const tinygltf::Primitive*>(GLTFPrimitivePtr);

    // ------------------- POSITION (mandatory) -------------------
    auto posIt = Prim->attributes.find("POSITION");
    if (posIt == Prim->attributes.end()) {
        SOLSTICE_LOG("AssetLoader: Primitive has no POSITION attribute");
        return;
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
}

// --------------------------------------------------------------------------
//  Material conversion
// --------------------------------------------------------------------------
Core::Material AssetLoader::ConvertMaterial(const void* GLTFModelPtr, int MaterialIndex) {
    const tinygltf::Model* Model = static_cast<const tinygltf::Model*>(GLTFModelPtr);
    Core::Material Mat;

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

} // namespace Solstice::Core
