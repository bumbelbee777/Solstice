#pragma once

#include "../../Generator.hxx"
#include "../../MeshData.hxx"
#include "../../Seed.hxx"

namespace Solstice::Arzachel {

// Architecture presets
SOLSTICE_API Generator<MeshData> Wall(const Seed& SeedParam, float Width, float Height);
SOLSTICE_API Generator<MeshData> Door(const Seed& SeedParam, float Width, float Height, int Type = 0); // 0 = single, 1 = double, 2 = sliding, 3 = security
SOLSTICE_API Generator<MeshData> Window(const Seed& SeedParam, float Width, float Height, int Type = 0); // 0 = standard, 1 = reinforced, 2 = barred
SOLSTICE_API Generator<MeshData> Stairs(const Seed& SeedParam, int Steps, float Width, int Type = 0); // 0 = straight, 1 = spiral, 2 = L-shaped, 3 = U-shaped
SOLSTICE_API Generator<MeshData> Railing(const Seed& SeedParam, float Length, int Type = 0); // 0 = standard, 1 = industrial, 2 = decorative
SOLSTICE_API Generator<MeshData> Hallway(const Seed& SeedParam, float Length, float Width, float Height);
SOLSTICE_API Generator<MeshData> Corridor(const Seed& SeedParam, float Length, float Width, float Height);
SOLSTICE_API Generator<MeshData> Room(const Seed& SeedParam, float Width, float Depth, float Height, int Type = 0); // 0 = office, 1 = lab, 2 = storage

} // namespace Solstice::Arzachel

