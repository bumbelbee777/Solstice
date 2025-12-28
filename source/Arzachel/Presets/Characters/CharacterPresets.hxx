#pragma once

#include "../../Generator.hxx"
#include "../../MeshData.hxx"
#include "../../Seed.hxx"

namespace Solstice::Arzachel {

// Character presets
SOLSTICE_API Generator<MeshData> Scientist(const Seed& SeedParam, int Gender = 0); // 0 = male, 1 = female
SOLSTICE_API Generator<MeshData> Soldier(const Seed& SeedParam, int Type = 0); // 0 = standard, 1 = officer
SOLSTICE_API Generator<MeshData> JohnLeeman(const Seed& SeedParam); // Main character: American systems engineer, early 30s, slim, blond hair, light stubble

} // namespace Solstice::Arzachel

