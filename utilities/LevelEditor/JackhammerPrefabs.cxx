#include "JackhammerPrefabs.hxx"

#include <Smf/SmfBinary.hxx>
#include <Smf/SmfMapEditor.hxx>
#include <Smf/SmfUtil.hxx>

#include <filesystem>
#include <unordered_set>

namespace Jackhammer::Prefabs {

namespace {

using Solstice::Smf::FindEntityIndex;
using Solstice::Smf::SmfAttributeType;
using Solstice::Smf::SmfEntity;
using Solstice::Smf::SmfMap;
using Solstice::Smf::SmfProperty;
using Solstice::Smf::SmfVec3;
using Solstice::Smf::SmfValue;

std::string MakeUniqueEntityName(const SmfMap& map, const std::string& preferred) {
    std::string base = preferred.empty() ? std::string("entity") : preferred;
    if (!FindEntityIndex(map, base)) {
        return base;
    }
    for (int n = 2; n < 1000000; ++n) {
        std::string tryName = base + "_" + std::to_string(n);
        if (!FindEntityIndex(map, tryName)) {
            return tryName;
        }
    }
    return base + "_dup";
}

void OffsetEntityOrigins(SmfEntity& ent, const SmfVec3& d) {
    for (SmfProperty& pr : ent.Properties) {
        if ((pr.Key == "origin" || pr.Key == "position") && pr.Type == SmfAttributeType::Vec3) {
            if (auto* v = std::get_if<SmfVec3>(&pr.Value)) {
                v->x += d.x;
                v->y += d.y;
                v->z += d.z;
            }
        }
    }
}

} // namespace

bool SaveEntitiesToSmfPrefabFile(const SmfMap& source, const std::vector<int>& entityIndices, const char* pathUtf8, std::string& errOut) {
    errOut.clear();
    if (!pathUtf8 || pathUtf8[0] == '\0') {
        errOut = "Prefab: empty path.";
        return false;
    }
    SmfMap out{};
    std::unordered_set<int> seen;
    for (int ei : entityIndices) {
        if (ei < 0 || static_cast<size_t>(ei) >= source.Entities.size()) {
            errOut = "Prefab: invalid entity index in selection.";
            return false;
        }
        if (!seen.insert(ei).second) {
            continue;
        }
        out.Entities.push_back(source.Entities[static_cast<size_t>(ei)]);
    }
    if (out.Entities.empty()) {
        errOut = "Prefab: no entities to save.";
        return false;
    }
    Solstice::Smf::SmfError smfErr = Solstice::Smf::SmfError::None;
    if (!Solstice::Smf::SaveSmfToFile(std::filesystem::path(pathUtf8), out, &smfErr, false)) {
        errOut = std::string("Prefab: save failed: ") + Solstice::Smf::SmfErrorMessage(smfErr);
        return false;
    }
    return true;
}

bool AppendPrefabSmfIntoMap(SmfMap& map, const char* pathUtf8, const SmfVec3& worldOffset, std::string& errOut) {
    errOut.clear();
    if (!pathUtf8 || pathUtf8[0] == '\0') {
        errOut = "Prefab: empty path.";
        return false;
    }
    SmfMap loaded{};
    Solstice::Smf::SmfFileHeader hdr{};
    Solstice::Smf::SmfError smfErr = Solstice::Smf::SmfError::None;
    if (!Solstice::Smf::LoadSmfFromFile(std::filesystem::path(pathUtf8), loaded, &hdr, &smfErr)) {
        errOut = std::string("Prefab: load failed: ") + Solstice::Smf::SmfErrorMessage(smfErr);
        return false;
    }
    if (loaded.Entities.empty()) {
        errOut = "Prefab: file has no entities.";
        return false;
    }
    for (SmfEntity& ent : loaded.Entities) {
        ent.Name = MakeUniqueEntityName(map, ent.Name);
        OffsetEntityOrigins(ent, worldOffset);
        map.Entities.push_back(std::move(ent));
    }
    return true;
}

} // namespace Jackhammer::Prefabs
