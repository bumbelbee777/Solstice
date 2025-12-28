#pragma once

#include "../Solstice.hxx"
#include "../Math/Vector.hxx"
#include "../Math/Quaternion.hxx"
#include "Seed.hxx"
#include <vector>
#include <cstdint>

namespace Solstice::Arzachel {

// Structure data
struct StructureData {
    enum class Type {
        Shack,
        Cabin
    };

    Math::Vec3 Position;
    Math::Quaternion Rotation;
    Math::Vec3 Scale;
    Type StructureType;
};

// Generate structures with constraints
SOLSTICE_API std::vector<StructureData> GenerateStructures(
    uint32_t Count,
    float TerrainSize,
    uint32_t SeedVal);

} // namespace Solstice::Arzachel
