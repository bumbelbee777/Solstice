#pragma once

#include "../../Generator.hxx"
#include "../../MeshData.hxx"
#include "../../Seed.hxx"

namespace Solstice::Arzachel {

// Weapon presets for early '00s aesthetic
SOLSTICE_API Generator<MeshData> Pistol(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> Rifle(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> Shotgun(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> MeleeWeapon(const Seed& SeedParam, int Type); // 0 = knife, 1 = crowbar, 2 = baton

} // namespace Solstice::Arzachel

