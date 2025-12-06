#pragma once

#include <iostream>
#include <filesystem>
#include <string>
#include <sstream>
#include <vector>
#include <Math/Vector.hxx>

namespace Solstice::Core {

struct ObjMaterial {
    std::string Name;
    Math::Vec3 Ambient = Math::Vec3(0.2f, 0.2f, 0.2f);
    Math::Vec3 Diffuse = Math::Vec3(0.8f, 0.8f, 0.8f);
    Math::Vec3 Specular = Math::Vec3(0.0f, 0.0f, 0.0f);
    float Shininess = 0.0f;
    float Alpha = 1.0f;
    std::string DiffuseMap;
    std::string NormalMap;
};

struct ObjSubMesh {
    std::string MaterialName;
    uint32_t IndexStart = 0;
    uint32_t IndexCount = 0;
};

struct WavefrontData {
    std::vector<Math::Vec3> Positions;
    std::vector<Math::Vec3> Normals;
    std::vector<Math::Vec2> UVs;
    std::vector<uint32_t> Indices;
    std::vector<ObjSubMesh> SubMeshes;
    std::vector<ObjMaterial> Materials;
    bool Success = false;
    std::string Error;
};

class WavefrontParser {
public:
    static WavefrontData Parse(const std::filesystem::path& Path);
};

} // namespace Solstice::Core