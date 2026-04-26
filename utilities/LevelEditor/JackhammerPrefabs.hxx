#pragma once

#include <Smf/SmfTypes.hxx>

#include <cstdint>
#include <string>
#include <vector>

namespace Solstice::Smf {
struct SmfMap;
}

namespace Jackhammer::Prefabs {

/// Writes a **minimal** ``.smf`` containing only the listed entities (path table / spatial / gameplay extras cleared).
bool SaveEntitiesToSmfPrefabFile(const Solstice::Smf::SmfMap& source, const std::vector<int>& entityIndices, const char* pathUtf8,
    std::string& errOut);

/// Appends every entity from a prefab ``.smf`` into ``map``, offsetting ``origin`` / ``position`` ``Vec3`` properties by ``worldOffset``
/// and renaming to stay unique (same pattern as duplicate-entity in the editor).
bool AppendPrefabSmfIntoMap(Solstice::Smf::SmfMap& map, const char* pathUtf8, const Solstice::Smf::SmfVec3& worldOffset, std::string& errOut);

} // namespace Jackhammer::Prefabs
