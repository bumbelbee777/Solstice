#pragma once

#include "../Generator.hxx"
#include "../MeshData.hxx"
#include "../Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <vector>
#include <string>

namespace Solstice::Arzachel {

// Level composition presets for fast level design
// These combine multiple presets into complete room/space configurations

struct RoomConnection {
    std::string RoomName;
    Solstice::Math::Vec3 ConnectionPoint;
    int ConnectionType; // 0 = door, 1 = hallway, 2 = open
};

struct AssetPlacement {
    std::string AssetType;
    Solstice::Math::Vec3 Position;
    Solstice::Math::Quaternion Rotation;
    float Scale;
};

// Pre-configured room compositions
SOLSTICE_API Generator<MeshData> OfficeRoom(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> LabRoom(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> CorridorSection(const Seed& SeedParam, float Length, int Features = 0);
SOLSTICE_API Generator<MeshData> Stairwell(const Seed& SeedParam, int Floors, int Type = 0);
SOLSTICE_API Generator<MeshData> ControlRoom(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> StorageRoom(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> HallwayJunction(const Seed& SeedParam, int Type = 0); // 0 = T, 1 = cross, 2 = L

// Combination functions
SOLSTICE_API std::vector<MeshData> CombineRooms(
    const std::vector<Generator<MeshData>>& Rooms,
    const std::vector<RoomConnection>& Connections,
    const Seed& SeedParam
);

SOLSTICE_API std::vector<AssetPlacement> PlaceAssets(
    const Generator<MeshData>& Room,
    const std::vector<std::string>& AssetTypes,
    const Seed& SeedParam
);

SOLSTICE_API Generator<MeshData> GenerateFloor(
    const Seed& SeedParam,
    const std::vector<std::pair<std::string, Solstice::Math::Vec3>>& RoomLayout
);

} // namespace Solstice::Arzachel

