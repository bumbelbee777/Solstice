#include "MapSerializer.hxx"
#include "../Core/Serialization/JSON.hxx"
#include "../Material/Material.hxx"
#include "../Math/Vector.hxx"
#include "../Math/Quaternion.hxx"
#include "../Physics/Lighting/LightSource.hxx"
#include "../Core/Audio/Audio.hxx"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Solstice::Arzachel {

namespace {

Core::Audio::ReverbPresetType ReverbPresetFromString(const std::string& preset) {
    if (preset == "Room") return Core::Audio::ReverbPresetType::Room;
    if (preset == "Cave") return Core::Audio::ReverbPresetType::Cave;
    if (preset == "Hallway") return Core::Audio::ReverbPresetType::Hallway;
    if (preset == "Sewer") return Core::Audio::ReverbPresetType::Sewer;
    if (preset == "Industrial") return Core::Audio::ReverbPresetType::Industrial;
    return Core::Audio::ReverbPresetType::None;
}

} // namespace

Core::JSONValue MapSerializer::Serialize(const MapData& Map) {
    Core::JSONObject Root;

    // Version and metadata
    Root["Version"] = Map.Version;
    Root["Name"] = Map.Name;
    Root["Description"] = Map.Description;
    Root["Seed"] = static_cast<double>(Map.Seed);

    // Objects
    Core::JSONArray ObjectsArray;
    for (const auto& Obj : Map.Objects) {
        Core::JSONObject ObjJSON;
        ObjJSON["MeshID"] = static_cast<double>(Obj.MeshID);
        ObjJSON["Position"] = Core::JSONArray{
            Obj.Position.x, Obj.Position.y, Obj.Position.z
        };
        ObjJSON["Rotation"] = Core::JSONArray{
            Obj.Rotation.x, Obj.Rotation.y, Obj.Rotation.z, Obj.Rotation.w
        };
        ObjJSON["Scale"] = Core::JSONArray{
            Obj.Scale.x, Obj.Scale.y, Obj.Scale.z
        };
        ObjJSON["MaterialID"] = static_cast<double>(Obj.MaterialID);
        ObjJSON["HasPhysics"] = Obj.HasPhysics;
        ObjectsArray.push_back(Core::JSONValue(std::move(ObjJSON)));
    }
    Root["Objects"] = Core::JSONValue(std::move(ObjectsArray));

    // Materials
    Core::JSONArray MaterialsArray;
    for (const auto& Mat : Map.Materials) {
        MaterialsArray.push_back(Core::Material::Serialize(Mat));
    }
    Root["Materials"] = Core::JSONValue(std::move(MaterialsArray));

    // Lights
    Core::JSONArray LightsArray;
    for (const auto& Light : Map.Lights) {
        Core::JSONObject LightJSON;
        LightJSON["Position"] = Core::JSONArray{
            Light.Position.x, Light.Position.y, Light.Position.z
        };
        LightJSON["Color"] = Core::JSONArray{
            Light.Color.x, Light.Color.y, Light.Color.z
        };
        LightJSON["Intensity"] = Light.Intensity;
        LightJSON["Hue"] = Light.Hue;
        LightJSON["Attenuation"] = Light.Attenuation;
        LightsArray.push_back(Core::JSONValue(std::move(LightJSON)));
    }
    Root["Lights"] = Core::JSONValue(std::move(LightsArray));

    // Acoustic zones
    Core::JSONArray ZonesArray;
    for (const auto& zone : Map.AcousticZones) {
        Core::JSONObject ZoneJSON;
        ZoneJSON["Name"] = zone.Name;
        ZoneJSON["Center"] = Core::JSONArray{zone.Center.x, zone.Center.y, zone.Center.z};
        ZoneJSON["Extents"] = Core::JSONArray{zone.Extents.x, zone.Extents.y, zone.Extents.z};
        ZoneJSON["ReverbPreset"] = zone.ReverbPreset;
        ZoneJSON["Wetness"] = zone.Wetness;
        ZoneJSON["ObstructionMultiplier"] = zone.ObstructionMultiplier;
        ZoneJSON["Priority"] = static_cast<double>(zone.Priority);
        ZoneJSON["IsSpherical"] = zone.IsSpherical;
        ZoneJSON["Enabled"] = zone.Enabled;
        ZonesArray.push_back(Core::JSONValue(std::move(ZoneJSON)));
    }
    Root["AcousticZones"] = Core::JSONValue(std::move(ZonesArray));

    // Metadata
    Core::JSONObject MetaJSON;
    MetaJSON["Author"] = Map.Meta.Author;
    MetaJSON["CreatedDate"] = Map.Meta.CreatedDate;
    MetaJSON["ModifiedDate"] = Map.Meta.ModifiedDate;
    Root["Metadata"] = Core::JSONValue(std::move(MetaJSON));

    return Core::JSONValue(std::move(Root));
}

MapData MapSerializer::Deserialize(const Core::JSONValue& JSON) {
    MapData Map;

    if (JSON.HasKey("Version")) Map.Version = JSON["Version"].AsString();
    if (JSON.HasKey("Name")) Map.Name = JSON["Name"].AsString();
    if (JSON.HasKey("Description")) Map.Description = JSON["Description"].AsString();
    if (JSON.HasKey("Seed")) Map.Seed = static_cast<uint32_t>(JSON["Seed"].AsDouble());

    // Objects
    if (JSON.HasKey("Objects") && JSON["Objects"].IsArray()) {
        const auto& ObjectsArray = JSON["Objects"].AsArray();
        for (const auto& ObjVal : ObjectsArray) {
            if (ObjVal.IsObject()) {
                MapData::ObjectData Obj;
                const auto& ObjJSON = ObjVal.AsObject();

                if (ObjJSON.find("MeshID") != ObjJSON.end()) {
                    Obj.MeshID = static_cast<uint32_t>(ObjJSON.at("MeshID").AsDouble());
                }
                if (ObjJSON.find("Position") != ObjJSON.end() && ObjJSON.at("Position").IsArray()) {
                    const auto& PosArray = ObjJSON.at("Position").AsArray();
                    if (PosArray.size() >= 3) {
                        Obj.Position = Math::Vec3(
                            PosArray[0].AsDouble(),
                            PosArray[1].AsDouble(),
                            PosArray[2].AsDouble()
                        );
                    }
                }
                if (ObjJSON.find("Rotation") != ObjJSON.end() && ObjJSON.at("Rotation").IsArray()) {
                    const auto& RotArray = ObjJSON.at("Rotation").AsArray();
                    if (RotArray.size() >= 4) {
                        Obj.Rotation = Math::Quaternion(
                            RotArray[0].AsDouble(),
                            RotArray[1].AsDouble(),
                            RotArray[2].AsDouble(),
                            RotArray[3].AsDouble()
                        );
                    }
                }
                if (ObjJSON.find("Scale") != ObjJSON.end() && ObjJSON.at("Scale").IsArray()) {
                    const auto& ScaleArray = ObjJSON.at("Scale").AsArray();
                    if (ScaleArray.size() >= 3) {
                        Obj.Scale = Math::Vec3(
                            ScaleArray[0].AsDouble(),
                            ScaleArray[1].AsDouble(),
                            ScaleArray[2].AsDouble()
                        );
                    }
                }
                if (ObjJSON.find("MaterialID") != ObjJSON.end()) {
                    Obj.MaterialID = static_cast<uint32_t>(ObjJSON.at("MaterialID").AsDouble());
                }
                if (ObjJSON.find("HasPhysics") != ObjJSON.end()) {
                    Obj.HasPhysics = ObjJSON.at("HasPhysics").AsBool();
                }

                Map.Objects.push_back(Obj);
            }
        }
    }

    // Materials
    if (JSON.HasKey("Materials") && JSON["Materials"].IsArray()) {
        const auto& MaterialsArray = JSON["Materials"].AsArray();
        for (const auto& MatVal : MaterialsArray) {
            try {
                Map.Materials.push_back(Core::Material::Deserialize(MatVal));
            } catch (...) {
                // Skip invalid materials
            }
        }
    }

    // Lights
    if (JSON.HasKey("Lights") && JSON["Lights"].IsArray()) {
        const auto& LightsArray = JSON["Lights"].AsArray();
        for (const auto& LightVal : LightsArray) {
            if (LightVal.IsObject()) {
                Physics::LightSource Light;
                const auto& LightJSON = LightVal.AsObject();

                if (LightJSON.find("Position") != LightJSON.end() && LightJSON.at("Position").IsArray()) {
                    const auto& PosArray = LightJSON.at("Position").AsArray();
                    if (PosArray.size() >= 3) {
                        Light.Position = Math::Vec3(
                            PosArray[0].AsDouble(),
                            PosArray[1].AsDouble(),
                            PosArray[2].AsDouble()
                        );
                    }
                }
                if (LightJSON.find("Color") != LightJSON.end() && LightJSON.at("Color").IsArray()) {
                    const auto& ColorArray = LightJSON.at("Color").AsArray();
                    if (ColorArray.size() >= 3) {
                        Light.Color = Math::Vec3(
                            ColorArray[0].AsDouble(),
                            ColorArray[1].AsDouble(),
                            ColorArray[2].AsDouble()
                        );
                    }
                }
                if (LightJSON.find("Intensity") != LightJSON.end()) {
                    Light.Intensity = static_cast<float>(LightJSON.at("Intensity").AsDouble());
                }
                if (LightJSON.find("Hue") != LightJSON.end()) {
                    Light.Hue = static_cast<float>(LightJSON.at("Hue").AsDouble());
                }
                if (LightJSON.find("Attenuation") != LightJSON.end()) {
                    Light.Attenuation = static_cast<float>(LightJSON.at("Attenuation").AsDouble());
                }

                Map.Lights.push_back(Light);
            }
        }
    }

    // Acoustic zones
    if (JSON.HasKey("AcousticZones") && JSON["AcousticZones"].IsArray()) {
        const auto& ZonesArray = JSON["AcousticZones"].AsArray();
        for (const auto& ZoneVal : ZonesArray) {
            if (!ZoneVal.IsObject()) {
                continue;
            }
            MapData::AcousticZoneData zone;
            const auto& ZoneJSON = ZoneVal.AsObject();
            if (ZoneJSON.find("Name") != ZoneJSON.end()) {
                zone.Name = ZoneJSON.at("Name").AsString();
            }
            if (ZoneJSON.find("Center") != ZoneJSON.end() && ZoneJSON.at("Center").IsArray()) {
                const auto& center = ZoneJSON.at("Center").AsArray();
                if (center.size() >= 3) {
                    zone.Center = Math::Vec3(center[0].AsDouble(), center[1].AsDouble(), center[2].AsDouble());
                }
            }
            if (ZoneJSON.find("Extents") != ZoneJSON.end() && ZoneJSON.at("Extents").IsArray()) {
                const auto& extents = ZoneJSON.at("Extents").AsArray();
                if (extents.size() >= 3) {
                    zone.Extents = Math::Vec3(extents[0].AsDouble(), extents[1].AsDouble(), extents[2].AsDouble());
                }
            }
            if (ZoneJSON.find("ReverbPreset") != ZoneJSON.end()) {
                zone.ReverbPreset = ZoneJSON.at("ReverbPreset").AsString();
            }
            if (ZoneJSON.find("Wetness") != ZoneJSON.end()) {
                zone.Wetness = static_cast<float>(ZoneJSON.at("Wetness").AsDouble());
            }
            if (ZoneJSON.find("ObstructionMultiplier") != ZoneJSON.end()) {
                zone.ObstructionMultiplier = static_cast<float>(ZoneJSON.at("ObstructionMultiplier").AsDouble());
            }
            if (ZoneJSON.find("Priority") != ZoneJSON.end()) {
                zone.Priority = static_cast<int>(ZoneJSON.at("Priority").AsDouble());
            }
            if (ZoneJSON.find("IsSpherical") != ZoneJSON.end()) {
                zone.IsSpherical = ZoneJSON.at("IsSpherical").AsBool();
            }
            if (ZoneJSON.find("Enabled") != ZoneJSON.end()) {
                zone.Enabled = ZoneJSON.at("Enabled").AsBool();
            }
            Map.AcousticZones.push_back(zone);
        }
    }

    // Metadata
    if (JSON.HasKey("Metadata") && JSON["Metadata"].IsObject()) {
        const auto& MetaJSON = JSON["Metadata"].AsObject();
        if (MetaJSON.find("Author") != MetaJSON.end()) {
            Map.Meta.Author = MetaJSON.at("Author").AsString();
        }
        if (MetaJSON.find("CreatedDate") != MetaJSON.end()) {
            Map.Meta.CreatedDate = MetaJSON.at("CreatedDate").AsString();
        }
        if (MetaJSON.find("ModifiedDate") != MetaJSON.end()) {
            Map.Meta.ModifiedDate = MetaJSON.at("ModifiedDate").AsString();
        }
    }

    return Map;
}

bool MapSerializer::SaveToFile(const std::string& FilePath, const MapData& Map) {
    try {
        std::filesystem::path Path(FilePath);
        if (Path.has_parent_path()) {
            std::filesystem::create_directories(Path.parent_path());
        }

        std::ofstream File(FilePath);
        if (!File.is_open()) {
            return false;
        }

        Core::JSONValue JSON = Serialize(Map);
        std::string JSONString = JSON.Stringify(true);
        File << JSONString;

        if (!File.good()) {
            return false;
        }

        File.close();
        return true;
    } catch (...) {
        return false;
    }
}

bool MapSerializer::LoadFromFile(const std::string& FilePath, MapData& OutMap) {
    std::ifstream File(FilePath);
    if (!File.is_open()) return false;

    std::stringstream Buffer;
    Buffer << File.rdbuf();

    try {
        Core::JSONValue JSON = Core::JSONParser::Parse(Buffer.str());

        // Check version compatibility
        if (JSON.HasKey("Version")) {
            std::string Version = JSON["Version"].AsString();
            if (!IsCompatibleVersion(Version)) {
                return false; // Incompatible version
            }
        }

        OutMap = Deserialize(JSON);
        std::vector<Core::Audio::AcousticZone> runtimeZones;
        runtimeZones.reserve(OutMap.AcousticZones.size());
        for (const auto& zone : OutMap.AcousticZones) {
            Core::Audio::AcousticZone runtimeZone;
            runtimeZone.Name = zone.Name;
            runtimeZone.Center = zone.Center;
            runtimeZone.Extents = zone.Extents;
            runtimeZone.Preset = ReverbPresetFromString(zone.ReverbPreset);
            runtimeZone.Wetness = zone.Wetness;
            runtimeZone.ObstructionMultiplier = zone.ObstructionMultiplier;
            runtimeZone.Priority = zone.Priority;
            runtimeZone.IsSpherical = zone.IsSpherical;
            runtimeZone.Enabled = zone.Enabled;
            runtimeZones.push_back(runtimeZone);
        }
        Core::Audio::AudioManager::Instance().SetAcousticZones(runtimeZones);
        return true;
    } catch (...) {
        return false;
    }
}

bool MapSerializer::IsCompatibleVersion(const std::string& Version) {
    return Version == "1.0" || Version == "1.1";
}

} // namespace Solstice::Arzachel

