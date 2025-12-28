#include "StructureGenerator.hxx"
#include "../Math/Vector.hxx"
#include "../Math/Quaternion.hxx"
#include "AssetPresets.hxx"
#include "Seed.hxx"
#include <cmath>

namespace Solstice::Arzachel {

std::vector<StructureData> GenerateStructures(uint32_t Count, float TerrainSize, uint32_t SeedVal) {
    std::vector<StructureData> Structures;
    Seed ArzSeed(SeedVal);

    for (uint32_t I = 0; I < Count; ++I) {
        Seed StructSeed = ArzSeed.Derive(I);
        float X = (static_cast<float>(StructSeed.Derive(0).Value % 1000) / 1000.0f) * TerrainSize - TerrainSize * 0.5f;
        float Z = (static_cast<float>(StructSeed.Derive(1).Value % 1000) / 1000.0f) * TerrainSize - TerrainSize * 0.5f;
        float RotY = (static_cast<float>(StructSeed.Derive(2).Value % 360));

        StructureData::Type Type = (StructSeed.Derive(3).Value % 2 == 0) ? StructureData::Type::Shack : StructureData::Type::Cabin;

        Structures.push_back({
            Math::Vec3(X, 0.0f, Z),
            Math::Quaternion::FromEuler(0.0f, RotY * 0.0174533f, 0.0f),
            Math::Vec3(1.0f, 1.0f, 1.0f),
            Type
        });
    }
    return Structures;
}

} // namespace Solstice::Arzachel
