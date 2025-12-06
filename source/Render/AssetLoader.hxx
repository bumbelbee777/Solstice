#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <Math/Vector.hxx>

namespace Solstice::Render {

// Forward declarations
class Mesh;
class Material;
class MeshLibrary;
class MaterialLibrary;

// Extended vertex data for glTF assets with additional attributes
struct ExtendedVertexData {
    std::vector<Math::Vec4> VertexColors;  // RGBA vertex colors (if present)
    std::vector<Math::Vec4> Tangents;      // XYZW tangents (W = handedness)
    bool HasVertexColors = false;
    bool HasTangents = false;
};

// Camera data from glTF
struct CameraData {
    std::string Name;
    bool IsPerspective = true;  // true = perspective, false = orthographic
    
    // Perspective parameters
    float Fov = 45.0f;          // Field of view in degrees
    float AspectRatio = 1.777f; // Width/Height
    float ZNear = 0.1f;
    float ZFar = 1000.0f;
    
    // Orthographic parameters
    float XMag = 1.0f;          // Half-width
    float YMag = 1.0f;          // Half-height
    
    // Transform (from node)
    float Transform[16];         // 4x4 matrix (column-major)
    Math::Vec3 Position;
    Math::Vec3 Forward;
    Math::Vec3 Up;
};

// Light data from glTF (KHR_lights_punctual extension)
struct LightData {
    std::string Name;
    enum class Type {
        Point,
        Spot,
        Directional
    } LightType = Type::Point;
    
    Math::Vec3 Color = Math::Vec3(1, 1, 1);
    float Intensity = 1.0f;
    float Range = 0.0f;          // 0 = infinite
    
    // Spot light parameters
    float InnerConeAngle = 0.0f; // Radians
    float OuterConeAngle = 0.785f; // Radians (45 degrees default)
    
    // Transform (from node)
    Math::Vec3 Position;
    Math::Vec3 Direction;
};

// Asset load result - contains all loaded data from a glTF file
struct AssetLoadResult {
    std::vector<std::unique_ptr<Mesh>> Meshes;
    std::vector<ExtendedVertexData> MeshExtendedData; // Parallel array to Meshes
    std::vector<Material> Materials;
    std::vector<std::string> TexturePaths;
    std::vector<CameraData> Cameras;
    std::vector<LightData> Lights;
    
    // Scene hierarchy information
    struct Node {
        std::string Name;
        int MeshIndex = -1;
        int MaterialIndex = -1;
        int CameraIndex = -1;
        int LightIndex = -1;
        float Transform[16]; // 4x4 matrix in column-major order
        std::vector<int> Children;
    };
    std::vector<Node> Nodes;
    std::vector<int> RootNodes; // Indices into Nodes array
    
    bool Success = false;
    std::string ErrorMessage;
};

// Asset loader - handles loading glTF 2.0 files using tinygltf
class AssetLoader {
private:
    static std::filesystem::path m_AssetPath;
    static std::unordered_map<std::string, uint32_t> m_LoadedAssets;
    
public:
    // Set the base path for asset loading
    static void SetAssetPath(const std::filesystem::path& Path);
    
    // Get the current asset path
    static const std::filesystem::path& GetAssetPath();
    
    // Load a glTF file and return all contained data
    // Path can be relative to asset path or absolute
    static AssetLoadResult LoadGLTF(const std::filesystem::path& Path);
    
    // Load a glTF file directly into mesh and material libraries
    // Returns the number of meshes loaded, or -1 on error
    static int LoadGLTFIntoLibraries(
        const std::filesystem::path& Path,
        MeshLibrary& MeshLib,
        MaterialLibrary& MatLib
    );
    
    // Load just the first mesh from a glTF file
    // Useful for simple cases where you only want one mesh
    static std::unique_ptr<Mesh> LoadMesh(const std::filesystem::path& Path);
    
    // Load just the materials from a glTF file
    static std::vector<Material> LoadMaterials(const std::filesystem::path& Path);
    
    // Clear the cache of loaded assets
    static void ClearCache();
    
private:
    // Helper functions for converting glTF data to our formats
    static std::pair<std::unique_ptr<Mesh>, ExtendedVertexData> ConvertMesh(
        const void* GLTFModel,  // tinygltf::Model*
        int MeshIndex
    );
    
    static Material ConvertMaterial(
        const void* GLTFModel,  // tinygltf::Model*
        int MaterialIndex
    );

    static CameraData ConvertCamera(
        const void* GLTFModel, // tinygltf::Model*
        int CameraIndex
    );

    static LightData ConvertLight(
        const void* GLTFModel, // tinygltf::Model*
        int LightIndex
    );
    
    static void ExtractPrimitiveData(
        const void* GLTFModel,      // tinygltf::Model*
        const void* GLTFPrimitive,  // tinygltf::Primitive*
        Mesh& OutMesh,
        ExtendedVertexData& OutExtendedData,
        uint32_t& IndexOffset
    );
    
    // Resolve a path relative to the asset path
    static std::filesystem::path ResolvePath(const std::filesystem::path& Path);
};

} // namespace Solstice::Render