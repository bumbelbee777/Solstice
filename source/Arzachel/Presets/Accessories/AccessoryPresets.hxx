#pragma once

#include "../../Generator.hxx"
#include "../../MeshData.hxx"
#include "../../Seed.hxx"

namespace Solstice::Arzachel {

// Accessory presets for early '00s aesthetic
SOLSTICE_API Generator<MeshData> Backpack(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> Helmet(const Seed& SeedParam, int Type = 0); // 0 = safety, 1 = combat
SOLSTICE_API Generator<MeshData> Goggles(const Seed& SeedParam);
SOLSTICE_API Generator<MeshData> GasMask(const Seed& SeedParam);

} // namespace Solstice::Arzachel

