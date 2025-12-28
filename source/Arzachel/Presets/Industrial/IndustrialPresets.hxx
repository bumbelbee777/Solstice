#pragma once

#include "../../Generator.hxx"
#include "../../MeshData.hxx"
#include "../../Seed.hxx"

namespace Solstice::Arzachel {

// Industrial presets
SOLSTICE_API Generator<MeshData> Pipe(const Seed& SeedParam, float Length, float Radius);
SOLSTICE_API Generator<MeshData> ControlPanel(const Seed& SeedParam, float Width);
SOLSTICE_API Generator<MeshData> Machinery(const Seed& SeedParam, int Type);
SOLSTICE_API Generator<MeshData> Vent(const Seed& SeedParam, float Size);
SOLSTICE_API Generator<MeshData> Terminal(const Seed& SeedParam, int Type = 0);
SOLSTICE_API Generator<MeshData> Computer(const Seed& SeedParam, int Type = 0);
SOLSTICE_API Generator<MeshData> Monitor(const Seed& SeedParam, float Size = 1.0f, int Type = 0);
SOLSTICE_API Generator<MeshData> Light(const Seed& SeedParam, int Type = 0); // 0 = ceiling, 1 = wall, 2 = desk

} // namespace Solstice::Arzachel

