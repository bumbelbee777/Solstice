#pragma once

#include "SmfTypes.hxx"

#include <string>
#include <utility>
#include <vector>

namespace Solstice::Smf {

struct SmfProperty {
    std::string Key;
    SmfAttributeType Type{SmfAttributeType::Float};
    SmfValue Value;
};

struct SmfEntity {
    std::string Name;
    std::string ClassName;
    std::vector<SmfProperty> Properties;
};

struct SmfMap {
    std::vector<SmfEntity> Entities;
    std::vector<std::pair<std::string, uint64_t>> PathTable;

    void Clear() {
        Entities.clear();
        PathTable.clear();
    }
};

} // namespace Solstice::Smf
