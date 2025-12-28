#pragma once

#include "../../Generator.hxx"
#include "../../MeshData.hxx"
#include "../../Seed.hxx"

namespace Solstice::Arzachel {

// Furniture presets
SOLSTICE_API Generator<MeshData> Desk(const Seed& SeedParam, float Width = 1.2f, float Depth = 0.6f);
SOLSTICE_API Generator<MeshData> Chair(const Seed& SeedParam, int Type = 0); // 0 = office, 1 = stool
SOLSTICE_API Generator<MeshData> Table(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> Cabinet(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> FilingCabinet(const Seed& SeedParam);

} // namespace Solstice::Arzachel

