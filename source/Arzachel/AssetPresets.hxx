#pragma once

#include "../Solstice.hxx"
#include "Generator.hxx"
#include "MeshData.hxx"
#include "Seed.hxx"

namespace Solstice::Arzachel {

// High-level asset builders
SOLSTICE_API Generator<MeshData> Building(const Seed& SeedParam, int Floors = 3);
SOLSTICE_API Generator<MeshData> Car(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> Ship(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> PlaneAsset(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> Shack(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> Cabin(const Seed& SeedParam);

// Variation support
SOLSTICE_API Generator<MeshData> Damaged(const Generator<MeshData>& Mesh, const Seed& SeedParam, float Amount);

// Pine tree presets
SOLSTICE_API Generator<MeshData> PineTree(const Seed& SeedParam, float Height, bool SnowCovered);

// Road generation
SOLSTICE_API Generator<MeshData> Road(const Seed& SeedParam, float Length, float Width);

// Swiss building presets
SOLSTICE_API Generator<MeshData> SwissHouse(const Seed& SeedParam, int Floors);
SOLSTICE_API Generator<MeshData> SwissChurch(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> SwissTownHall(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> SwissShop(const Seed& SeedParam);

// Ground-level vegetation presets
SOLSTICE_API Generator<MeshData> GrassPatch(const Seed& SeedParam, float Size);
SOLSTICE_API Generator<MeshData> SmallBush(const Seed& SeedParam, float Height);
SOLSTICE_API Generator<MeshData> Rock(const Seed& SeedParam, float Size);
SOLSTICE_API Generator<MeshData> SnowDrift(const Seed& SeedParam, float Width, float Height);

} // namespace Solstice::Arzachel
