#pragma once

#include "../Solstice.hxx"
#include "../Core/Serialization/JSON.hxx"
#include <Render/Scene/Scene.hxx>
#include "../Material/Material.hxx"
#include "../Physics/Lighting/LightSource.hxx"
#include "../Arzachel/Seed.hxx"
#include <Smf/SmfMap.hxx>
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

    // Acoustic zones for cinematic spatial audio.
    struct AcousticZoneData {
        std::string Name;
        Math::Vec3 Center;
        Math::Vec3 Extents{5.0f, 5.0f, 5.0f};
        std::string ReverbPreset{"Room"};
        float Wetness{0.35f};
        float ObstructionMultiplier{1.0f};
        int Priority{0};
        bool IsSpherical{false};
        bool Enabled{true};
        std::string MusicPath;
        std::string AmbiencePath;
    };
    std::vector<AcousticZoneData> AcousticZones;
    
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
    static std::string GetCurrentVersion() { return "1.1"; }

    /// Push SMF **gameplay** extras into engine state: ``AudioManager`` acoustic zones; authoring lights cached for render.
    static void ApplyGameplayFromSmfMap(const Solstice::Smf::SmfMap& Map);
    static const std::vector<Physics::LightSource>& GetAuthoringLightsFromLastSmfApply();
};

} // namespace Solstice::Arzachel

