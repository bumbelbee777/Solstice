#include "MapSerializer.hxx"
#include "../Core/Serialization/JSON.hxx"
#include "../Material/Material.hxx"
#include "../Math/Vector.hxx"
#include "../Math/Quaternion.hxx"
#include "../Physics/Lighting/LightSource.hxx"
#include <Physics/Fluid/Fluid.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include "../Core/Audio/Audio.hxx"
#include <Core/AuthoringSkyboxBus.hxx>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <vector>

static std::vector<Solstice::Physics::LightSource> g_AuthoringLightsFromLastSmfApply;
static std::vector<std::unique_ptr<Solstice::Physics::FluidSimulation>> g_MapAuthoredFluids;
static uint64_t g_LastFluidFingerprint = 0xFFFFFFFFFFFFFFFFull;

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

inline uint64_t MixU64(uint64_t h, uint64_t x) {
    h ^= x;
    return h * 1099511628211ull;
}

inline uint32_t FloatBits(float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(f));
    return u;
}

uint64_t HashFluidAuthoring(const Solstice::Smf::SmfMap& map) {
    uint64_t h = 1469598103934665603ull;
    h = MixU64(h, static_cast<uint64_t>(map.FluidVolumes.size()));
    for (const auto& f : map.FluidVolumes) {
        h = MixU64(h, f.Enabled ? 1ull : 0ull);
        h = MixU64(h, static_cast<uint64_t>(std::hash<std::string>{}(f.Name)));
        h = MixU64(h, FloatBits(f.BoundsMin.x));
        h = MixU64(h, FloatBits(f.BoundsMin.y));
        h = MixU64(h, FloatBits(f.BoundsMin.z));
        h = MixU64(h, FloatBits(f.BoundsMax.x));
        h = MixU64(h, FloatBits(f.BoundsMax.y));
        h = MixU64(h, FloatBits(f.BoundsMax.z));
        h = MixU64(h, static_cast<uint64_t>(static_cast<uint32_t>(f.ResolutionX)));
        h = MixU64(h, static_cast<uint64_t>(static_cast<uint32_t>(f.ResolutionY)));
        h = MixU64(h, static_cast<uint64_t>(static_cast<uint32_t>(f.ResolutionZ)));
        h = MixU64(h, FloatBits(f.Diffusion));
        h = MixU64(h, FloatBits(f.Viscosity));
        h = MixU64(h, FloatBits(f.ReferenceDensity));
        h = MixU64(h, static_cast<uint64_t>(static_cast<uint32_t>(f.PressureRelaxationIterations)));
        h = MixU64(h, FloatBits(f.BuoyancyStrength));
        h = MixU64(h, FloatBits(f.Prandtl));
        h = MixU64(h, f.EnableMacCormack ? 1ull : 0ull);
        h = MixU64(h, f.EnableBoussinesq ? 1ull : 0ull);
        h = MixU64(h, f.VolumeVisualizationClip ? 1ull : 0ull);
    }
    return h;
}

void RebuildMapFluidsFromSmf(const Solstice::Smf::SmfMap& map) {
    Solstice::Physics::PhysicsSystem& ps = Solstice::Physics::PhysicsSystem::Instance();
    for (auto& up : g_MapAuthoredFluids) {
        if (up) {
            ps.UnregisterFluidSimulation(up.get());
        }
    }
    g_MapAuthoredFluids.clear();

    for (const auto& src : map.FluidVolumes) {
        if (!src.Enabled) {
            continue;
        }
        int nx = static_cast<int>(std::clamp(src.ResolutionX, Solstice::Smf::kSmfFluidResolutionMin, Solstice::Smf::kSmfFluidResolutionMax));
        int ny = static_cast<int>(std::clamp(src.ResolutionY, Solstice::Smf::kSmfFluidResolutionMin, Solstice::Smf::kSmfFluidResolutionMax));
        int nz = static_cast<int>(std::clamp(src.ResolutionZ, Solstice::Smf::kSmfFluidResolutionMin, Solstice::Smf::kSmfFluidResolutionMax));
        while (static_cast<int64_t>(nx) * static_cast<int64_t>(ny) * static_cast<int64_t>(nz) > Solstice::Smf::kSmfFluidInteriorCellBudget
            && (nx > Solstice::Smf::kSmfFluidResolutionMin || ny > Solstice::Smf::kSmfFluidResolutionMin
                || nz > Solstice::Smf::kSmfFluidResolutionMin)) {
            if (nx >= ny && nx >= nz && nx > Solstice::Smf::kSmfFluidResolutionMin) {
                --nx;
            } else if (ny >= nz && ny > Solstice::Smf::kSmfFluidResolutionMin) {
                --ny;
            } else if (nz > Solstice::Smf::kSmfFluidResolutionMin) {
                --nz;
            } else {
                break;
            }
        }
        if (nx <= 0 || ny <= 0 || nz <= 0) {
            continue;
        }

        const float ax0 = std::min(src.BoundsMin.x, src.BoundsMax.x);
        const float ay0 = std::min(src.BoundsMin.y, src.BoundsMax.y);
        const float az0 = std::min(src.BoundsMin.z, src.BoundsMax.z);
        const float ax1 = std::max(src.BoundsMin.x, src.BoundsMax.x);
        const float ay1 = std::max(src.BoundsMin.y, src.BoundsMax.y);
        const float az1 = std::max(src.BoundsMin.z, src.BoundsMax.z);
        const Solstice::Math::Vec3 mn(ax0, ay0, az0);
        const Solstice::Math::Vec3 mx(ax1, ay1, az1);
        Solstice::Math::Vec3 ext = mx - mn;
        const float minSpan = 1e-3f;
        ext.x = std::max(ext.x, minSpan);
        ext.y = std::max(ext.y, minSpan);
        ext.z = std::max(ext.z, minSpan);

        const float hx = ext.x / static_cast<float>(nx);
        const float hy = ext.y / static_cast<float>(ny);
        const float hz = ext.z / static_cast<float>(nz);

        float diff = src.Diffusion;
        float visc = src.Viscosity;
        if (!std::isfinite(diff)) {
            diff = 1e-4f;
        }
        if (!std::isfinite(visc)) {
            visc = 1e-4f;
        }
        diff = std::clamp(diff, 0.f, 50.f);
        visc = std::clamp(visc, 1e-8f, 50.f);

        auto sim = std::make_unique<Solstice::Physics::FluidSimulation>(nx, ny, nz, hx, hy, hz, diff, visc);
        sim->SetGridOrigin(mn);
        const float refD = std::isfinite(src.ReferenceDensity) ? src.ReferenceDensity : 1000.f;
        sim->SetReferenceDensity(std::max(refD, 1.f));
        sim->GetTuning().enableMacCormack = src.EnableMacCormack;
        const int pr = static_cast<int>(src.PressureRelaxationIterations);
        sim->GetTuning().pressureRelaxationIterations = std::clamp(pr, 8, 64);
        if (src.EnableBoussinesq) {
            Solstice::Physics::FluidThermalTuning& th = sim->GetThermal();
            th.enableBoussinesq = true;
            const float bStr = std::isfinite(src.BuoyancyStrength) ? src.BuoyancyStrength : 1.f;
            th.buoyancyStrength = std::clamp(bStr, 0.f, 1.0e6f);
            const float prl = std::isfinite(src.Prandtl) ? src.Prandtl : 0.71f;
            th.Prandtl = std::clamp(prl, 1e-6f, 1000.f);
        }
        if (src.VolumeVisualizationClip) {
            sim->SetVolumeVisualizationClip(true, mn, mx);
        }
        ps.RegisterFluidSimulation(sim.get());
        g_MapAuthoredFluids.push_back(std::move(sim));
    }
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
        if (!zone.MusicPath.empty()) {
            ZoneJSON["MusicPath"] = zone.MusicPath;
        }
        if (!zone.AmbiencePath.empty()) {
            ZoneJSON["AmbiencePath"] = zone.AmbiencePath;
        }
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
            if (ZoneJSON.find("MusicPath") != ZoneJSON.end()) {
                zone.MusicPath = ZoneJSON.at("MusicPath").AsString();
            }
            if (ZoneJSON.find("AmbiencePath") != ZoneJSON.end()) {
                zone.AmbiencePath = ZoneJSON.at("AmbiencePath").AsString();
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
            runtimeZone.MusicPath = zone.MusicPath;
            runtimeZone.AmbiencePath = zone.AmbiencePath;
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

void MapSerializer::ApplyGameplayFromSmfMap(const Solstice::Smf::SmfMap& Map) {
    std::vector<Core::Audio::AcousticZone> runtimeZones;
    runtimeZones.reserve(Map.AcousticZones.size());
    for (const auto& z : Map.AcousticZones) {
        Core::Audio::AcousticZone runtimeZone;
        runtimeZone.Name = z.Name;
        runtimeZone.Center = Math::Vec3(z.Center.x, z.Center.y, z.Center.z);
        runtimeZone.Extents = Math::Vec3(z.Extents.x, z.Extents.y, z.Extents.z);
        runtimeZone.Preset = ReverbPresetFromString(z.ReverbPreset);
        runtimeZone.Wetness = z.Wetness;
        runtimeZone.ObstructionMultiplier = z.ObstructionMultiplier;
        runtimeZone.Priority = z.Priority;
        runtimeZone.IsSpherical = z.IsSpherical;
        runtimeZone.Enabled = z.Enabled;
        runtimeZone.MusicPath = z.MusicPath;
        runtimeZone.AmbiencePath = z.AmbiencePath;
        runtimeZones.push_back(runtimeZone);
    }
    Core::Audio::AudioManager::Instance().SetAcousticZones(runtimeZones);

    g_AuthoringLightsFromLastSmfApply.clear();
    g_AuthoringLightsFromLastSmfApply.reserve(Map.AuthoringLights.size());
    for (const auto& L : Map.AuthoringLights) {
        Physics::LightSource ls;
        switch (L.Type) {
        case Solstice::Smf::SmfAuthoringLightType::Point:
            ls.Type = Physics::LightSource::LightType::Point;
            break;
        case Solstice::Smf::SmfAuthoringLightType::Spot:
            ls.Type = Physics::LightSource::LightType::Spot;
            break;
        case Solstice::Smf::SmfAuthoringLightType::Directional:
            ls.Type = Physics::LightSource::LightType::Directional;
            break;
        default:
            ls.Type = Physics::LightSource::LightType::Point;
            break;
        }
        ls.Position = Math::Vec3(L.Position.x, L.Position.y, L.Position.z);
        if (L.Type == Solstice::Smf::SmfAuthoringLightType::Directional) {
            ls.Position = Math::Vec3(L.Direction.x, L.Direction.y, L.Direction.z).Normalized();
        }
        ls.Color = Math::Vec3(L.Color.x, L.Color.y, L.Color.z);
        ls.Intensity = L.Intensity;
        ls.Hue = L.Hue;
        ls.Attenuation = L.Attenuation;
        ls.Range = L.Range;
        g_AuthoringLightsFromLastSmfApply.push_back(ls);
    }

    const uint64_t fluidFp = HashFluidAuthoring(Map);
    if (fluidFp != g_LastFluidFingerprint) {
        g_LastFluidFingerprint = fluidFp;
        RebuildMapFluidsFromSmf(Map);
    }

    if (Map.Skybox.has_value()) {
        const auto& sk = *Map.Skybox;
        Core::AuthoringSkyboxState pub{};
        pub.Enabled = sk.Enabled;
        pub.Brightness = sk.Brightness;
        pub.YawDegrees = sk.YawDegrees;
        for (int i = 0; i < 6; ++i) {
            pub.FacePaths[static_cast<size_t>(i)] = sk.FacePaths[i];
        }
        Core::PublishAuthoringSkyboxState(pub);
    }
}

const std::vector<Physics::LightSource>& MapSerializer::GetAuthoringLightsFromLastSmfApply() {
    return g_AuthoringLightsFromLastSmfApply;
}

} // namespace Solstice::Arzachel

