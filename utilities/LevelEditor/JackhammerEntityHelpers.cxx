#include "JackhammerEntityHelpers.hxx"

#include <Smf/SmfMapEditor.hxx>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Jackhammer::EntityHelpers {

using Solstice::Smf::FindEntityIndex;
using Solstice::Smf::SmfAttributeType;
using Solstice::Smf::SmfEntity;
using Solstice::Smf::SmfMap;
using Solstice::Smf::SmfProperty;
using Solstice::Smf::SmfValue;
using Solstice::Smf::SmfVec3;

namespace {

const char* kDiffuseTextureKeys[] = {"diffuseTexture", "albedoTexture", "texture"};
const char* kMaterialPathKeys[] = {"materialPath", "smatPath"};
const char* kNormalTextureKeys[] = {"normalTexture", "normalMap"};
const char* kRoughnessTextureKeys[] = {"roughnessTexture", "roughnessMap"};
const char* kModelAssetKeys[] = {"modelPath", "meshPath", "gltfPath"};

const SmfVec3* TryGetEntityVec3ByKeyImpl(const SmfEntity& ent, const char* key) {
    for (const auto& pr : ent.Properties) {
        if (pr.Key == key && pr.Type == SmfAttributeType::Vec3) {
            if (auto* v = std::get_if<SmfVec3>(&pr.Value)) {
                return v;
            }
        }
    }
    return nullptr;
}

float TryGetEntityFloatByKeyImpl(const SmfEntity& ent, const char* key, float defaultVal) {
    for (const auto& pr : ent.Properties) {
        if (pr.Key == key && pr.Type == SmfAttributeType::Float) {
            if (auto* f = std::get_if<float>(&pr.Value)) {
                return *f;
            }
        }
    }
    return defaultVal;
}

} // namespace

const SmfVec3* TryGetEntityVec3ByKey(const SmfEntity& ent, const char* key) {
    return TryGetEntityVec3ByKeyImpl(ent, key);
}

float TryGetEntityFloatByKey(const SmfEntity& ent, const char* key, float defaultVal) {
    return TryGetEntityFloatByKeyImpl(ent, key, defaultVal);
}

std::string MakeUniqueEntityName(const SmfMap& map, const std::string& preferred) {
    std::string base = preferred.empty() ? std::string("entity") : preferred;
    if (!FindEntityIndex(map, base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        std::string c = base + "_" + std::to_string(i);
        if (!FindEntityIndex(map, c)) {
            return c;
        }
    }
    return base + "_dup";
}

std::string MakeUniqueAcousticZoneName(const SmfMap& map, const std::string& preferred) {
    const std::string base = preferred.empty() ? std::string("reverb_zone") : preferred;
    auto used = [&](const std::string& n) {
        for (const auto& z : map.AcousticZones) {
            if (z.Name == n) {
                return true;
            }
        }
        return false;
    };
    if (!used(base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        const std::string c = base + "_" + std::to_string(i);
        if (!used(c)) {
            return c;
        }
    }
    return base + "_dup";
}

std::string MakeUniqueAuthoringLightName(const SmfMap& map, const std::string& preferred) {
    const std::string base = preferred.empty() ? std::string("light") : preferred;
    auto used = [&](const std::string& n) {
        for (const auto& L : map.AuthoringLights) {
            if (L.Name == n) {
                return true;
            }
        }
        return false;
    };
    if (!used(base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        const std::string c = base + "_" + std::to_string(i);
        if (!used(c)) {
            return c;
        }
    }
    return base + "_dup";
}

std::string MakeUniqueFluidVolumeName(const SmfMap& map, const std::string& preferred) {
    const std::string base = preferred.empty() ? std::string("fluid_volume") : preferred;
    auto used = [&](const std::string& n) {
        for (const auto& f : map.FluidVolumes) {
            if (f.Name == n) {
                return true;
            }
        }
        return false;
    };
    if (!used(base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        const std::string c = base + "_" + std::to_string(i);
        if (!used(c)) {
            return c;
        }
    }
    return base + "_dup";
}

std::string MakeUniqueSoftBodyVolumeName(const SmfMap& map, const std::string& preferred) {
    const std::string base = preferred.empty() ? std::string("soft_body") : preferred;
    auto used = [&](const std::string& n) {
        for (const auto& s : map.SoftBodyVolumes) {
            if (s.Name == n) {
                return true;
            }
        }
        return false;
    };
    if (!used(base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        const std::string c = base + "_" + std::to_string(i);
        if (!used(c)) {
            return c;
        }
    }
    return base + "_dup";
}

std::string MakeUniqueVehicleVolumeName(const SmfMap& map, const std::string& preferred) {
    const std::string base = preferred.empty() ? std::string("vehicle") : preferred;
    auto used = [&](const std::string& n) {
        for (const auto& v : map.VehicleVolumes) {
            if (v.Name == n) {
                return true;
            }
        }
        return false;
    };
    if (!used(base)) {
        return base;
    }
    for (int i = 2; i < 100000; ++i) {
        const std::string c = base + "_" + std::to_string(i);
        if (!used(c)) {
            return c;
        }
    }
    return base + "_dup";
}

const char* TryGetEntityDiffuseTexturePath(const SmfEntity& ent) {
    for (const char* key : kDiffuseTextureKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

void SetEntityDiffuseTexturePath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kDiffuseTextureKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kDiffuseTextureKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"diffuseTexture", SmfAttributeType::String, pathUtf8});
}

const char* TryGetEntityMaterialPath(const SmfEntity& ent) {
    for (const char* key : kMaterialPathKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

void SetEntityMaterialPath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kMaterialPathKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kMaterialPathKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"materialPath", SmfAttributeType::String, pathUtf8});
}

const char* TryGetEntityNormalTexturePath(const SmfEntity& ent) {
    for (const char* key : kNormalTextureKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

void SetEntityNormalTexturePath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kNormalTextureKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kNormalTextureKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"normalTexture", SmfAttributeType::String, pathUtf8});
}

const char* TryGetEntityRoughnessTexturePath(const SmfEntity& ent) {
    for (const char* key : kRoughnessTextureKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

void SetEntityRoughnessTexturePath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kRoughnessTextureKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kRoughnessTextureKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"roughnessTexture", SmfAttributeType::String, pathUtf8});
}

const char* TryGetEntityModelAssetPath(const SmfEntity& ent) {
    for (const char* key : kModelAssetKeys) {
        for (const auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                if (const auto* s = std::get_if<std::string>(&pr.Value)) {
                    if (!s->empty()) {
                        return s->c_str();
                    }
                }
            }
        }
    }
    return nullptr;
}

void SetEntityModelAssetPath(SmfEntity& ent, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        for (const char* key : kModelAssetKeys) {
            ent.Properties.erase(std::remove_if(ent.Properties.begin(), ent.Properties.end(),
                                     [key](const SmfProperty& p) { return p.Key == key && p.Type == SmfAttributeType::String; }),
                ent.Properties.end());
        }
        return;
    }
    for (const char* key : kModelAssetKeys) {
        for (auto& pr : ent.Properties) {
            if (pr.Key == key && pr.Type == SmfAttributeType::String) {
                pr.Value = pathUtf8;
                return;
            }
        }
    }
    ent.Properties.push_back({"modelPath", SmfAttributeType::String, pathUtf8});
}

std::string ToMapRelativePathIfPossible(const std::string& assetPathUtf8, const std::optional<std::string>& currentMapPath) {
    if (!currentMapPath || currentMapPath->empty()) {
        return assetPathUtf8;
    }
    std::error_code ec;
    const std::filesystem::path mapDir = std::filesystem::path(*currentMapPath).parent_path();
    const std::filesystem::path absAsset = std::filesystem::absolute(std::filesystem::path(assetPathUtf8), ec);
    if (ec || absAsset.empty() || mapDir.empty()) {
        return assetPathUtf8;
    }
    const std::filesystem::path rel = std::filesystem::relative(absAsset, mapDir, ec);
    if (ec || rel.empty()) {
        return assetPathUtf8;
    }
    return rel.generic_string();
}

std::string ResolveMapAssetPath(const std::optional<std::string>& currentMapPath, const std::string& pathUtf8) {
    if (pathUtf8.empty()) {
        return {};
    }
    std::error_code ec;
    const std::filesystem::path p(pathUtf8);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    }
    if (!currentMapPath || currentMapPath->empty()) {
        return pathUtf8;
    }
    const std::filesystem::path mapDir = std::filesystem::path(*currentMapPath).parent_path();
    if (mapDir.empty()) {
        return pathUtf8;
    }
    const std::filesystem::path joined = mapDir / p;
    const std::filesystem::path norm = std::filesystem::weakly_canonical(joined, ec);
    if (ec || norm.empty()) {
        return joined.lexically_normal().generic_string();
    }
    return norm.generic_string();
}

const SmfVec3* TryGetEntityOriginVec3(const SmfEntity& ent) {
    for (const auto& pr : ent.Properties) {
        if ((pr.Key == "origin" || pr.Key == "position") && pr.Type == SmfAttributeType::Vec3) {
            if (auto* v = std::get_if<SmfVec3>(&pr.Value)) {
                return v;
            }
        }
    }
    return nullptr;
}

bool ComputeEntityOriginAabb(const SmfMap& map, SmfVec3& outMin, SmfVec3& outMax) {
    bool any = false;
    for (const auto& ent : map.Entities) {
        const SmfVec3* o = TryGetEntityOriginVec3(ent);
        if (!o) {
            continue;
        }
        if (!any) {
            outMin = outMax = *o;
            any = true;
        } else {
            outMin.x = std::min(outMin.x, o->x);
            outMin.y = std::min(outMin.y, o->y);
            outMin.z = std::min(outMin.z, o->z);
            outMax.x = std::max(outMax.x, o->x);
            outMax.y = std::max(outMax.y, o->y);
            outMax.z = std::max(outMax.z, o->z);
        }
    }
    return any;
}

bool SetEntityOriginVec3(SmfEntity& ent, const SmfVec3& pos) {
    for (auto& pr : ent.Properties) {
        if ((pr.Key == "origin" || pr.Key == "position") && pr.Type == SmfAttributeType::Vec3) {
            pr.Value = SmfValue{pos};
            return true;
        }
    }
    ent.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{pos}});
    return true;
}

void SetEntityVec3ByKey(SmfEntity& ent, const char* key, const SmfVec3& v) {
    for (auto& pr : ent.Properties) {
        if (pr.Key == key && pr.Type == SmfAttributeType::Vec3) {
            pr.Value = SmfValue{v};
            return;
        }
    }
    ent.Properties.push_back({key, SmfAttributeType::Vec3, SmfValue{v}});
}

void SetEntityFloatByKey(SmfEntity& ent, const char* key, float f) {
    for (auto& pr : ent.Properties) {
        if (pr.Key == key && pr.Type == SmfAttributeType::Float) {
            pr.Value = SmfValue{f};
            return;
        }
    }
    ent.Properties.push_back({key, SmfAttributeType::Float, SmfValue{f}});
}

float ReadEntityYawDegrees(const SmfEntity& ent) {
    return TryGetEntityFloatByKey(ent, "yaw", TryGetEntityFloatByKey(ent, "YawDegrees", 0.f));
}

float ReadEntityPitchDegrees(const SmfEntity& ent) {
    return TryGetEntityFloatByKey(ent, "pitch", TryGetEntityFloatByKey(ent, "PitchDegrees", 0.f));
}

float ReadEntityRollDegrees(const SmfEntity& ent) {
    return TryGetEntityFloatByKey(ent, "roll", TryGetEntityFloatByKey(ent, "RollDegrees", 0.f));
}

SmfVec3 ReadEntityScaleVec3(const SmfEntity& ent) {
    if (const SmfVec3* sc = TryGetEntityVec3ByKeyImpl(ent, "scale")) {
        return *sc;
    }
    const float s = TryGetEntityFloatByKeyImpl(ent, "Scale", 1.0f);
    return SmfVec3{s, s, s};
}

float ReadEntityUniformScale(const SmfEntity& ent) {
    const SmfVec3* sc = TryGetEntityVec3ByKeyImpl(ent, "scale");
    if (sc) {
        return sc->x;
    }
    return TryGetEntityFloatByKeyImpl(ent, "Scale", 1.0f);
}

void SetEntityYawDegrees(SmfEntity& ent, float yawDeg) {
    SetEntityFloatByKey(ent, "yaw", yawDeg);
    SetEntityFloatByKey(ent, "YawDegrees", yawDeg);
}

void SetEntityPitchDegrees(SmfEntity& ent, float pitchDeg) {
    SetEntityFloatByKey(ent, "pitch", pitchDeg);
    SetEntityFloatByKey(ent, "PitchDegrees", pitchDeg);
}

void SetEntityRollDegrees(SmfEntity& ent, float rollDeg) {
    SetEntityFloatByKey(ent, "roll", rollDeg);
    SetEntityFloatByKey(ent, "RollDegrees", rollDeg);
}

void SetEntityUniformScale(SmfEntity& ent, float s) {
    const float clamped = std::clamp(s, 0.05f, 128.0f);
    SetEntityVec3ByKey(ent, "scale", SmfVec3{clamped, clamped, clamped});
    SetEntityFloatByKey(ent, "Scale", clamped);
}

void SetEntityScaleVec3(SmfEntity& ent, const SmfVec3& v) {
    SetEntityVec3ByKey(
        ent, "scale", SmfVec3{std::clamp(v.x, 0.05f, 128.0f), std::clamp(v.y, 0.05f, 128.0f), std::clamp(v.z, 0.05f, 128.0f)});
}

} // namespace Jackhammer::EntityHelpers
