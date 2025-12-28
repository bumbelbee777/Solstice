#pragma once

#include "../../Generator.hxx"
#include "../../MeshData.hxx"
#include "../../Seed.hxx"

namespace Solstice::Arzachel {

// Bullet presets
SOLSTICE_API Generator<MeshData> Bullet(const Seed& SeedParam, float Caliber = 9.0f); // Caliber in mm
SOLSTICE_API Generator<MeshData> Shell(const Seed& SeedParam, int Type = 0); // 0 = shotgun, 1 = rifle
SOLSTICE_API Generator<MeshData> Projectile(const Seed& SeedParam, int Type = 0); // Generic projectile

} // namespace Solstice::Arzachel

