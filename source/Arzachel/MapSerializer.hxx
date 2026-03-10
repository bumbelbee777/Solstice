#pragma once

#include "../Solstice.hxx"
#include "../Core/JSON.hxx"
#include <Render/Scene/Scene.hxx>
#include "../Core/Material.hxx"
#include "../Physics/LightSource.hxx"
#include "../Arzachel/Seed.hxx"
#include <string>
#include <vector>
#include <memory>

namespace Solstice::Arzachel {

// Map/Level data structure for serialization
struct MapData {
    std::string Version{"1.0"};
    std::string Name;
    std::string Description;
    uint32_t Seed{0};
    
    // Geometry and assets
    struct ObjectData {
        uint32_t MeshID;
        Math::Vec3 Position;
        Math::Quaternion Rotation;
        Math::Vec3 Scale;
        uint32_t MaterialID;
        bool HasPhysics{false};
    };
    std::vector<ObjectData> Objects;
    
    // Materials
    std::vector<Core::Material> Materials;
    
    // Lighting
    std::vector<Physics::LightSource> Lights;
    
    // Metadata
    struct Metadata {
        std::string Author;
        std::string CreatedDate;
        std::string ModifiedDate;
    } Meta;
};

// Map serializer for level data persistence
class SOLSTICE_API MapSerializer {
public:
    // Serialization
    static Core::JSONValue Serialize(const MapData& Map);
    static MapData Deserialize(const Core::JSONValue& JSON);
    
    // File I/O
    static bool SaveToFile(const std::string& FilePath, const MapData& Map);
    static bool LoadFromFile(const std::string& FilePath, MapData& OutMap);
    
    // Version management
    static bool IsCompatibleVersion(const std::string& Version);
    static std::string GetCurrentVersion() { return "1.0"; }
};

} // namespace Solstice::Arzachel

