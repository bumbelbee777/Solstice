#pragma once

#include "Generator.hxx"
#include "MeshData.hxx"
#include "Seed.hxx"
#include <Math/Vector.hxx>

namespace Solstice::Arzachel {

// Character modeling primitives for procedural character generation
// Early '00s aesthetic: blocky, low-poly, stylized

SOLSTICE_API Generator<MeshData> Head(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> Torso(const Seed& SeedParam, int Build = 0); // 0 = slim, 1 = average, 2 = muscular
SOLSTICE_API Generator<MeshData> Limb(const Seed& SeedParam, int Type, float Length); // 0 = arm, 1 = leg
SOLSTICE_API Generator<MeshData> Hand(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> Foot(const Seed& SeedParam, float Size = 1.0f);
SOLSTICE_API Generator<MeshData> Hair(const Seed& SeedParam, int Style, float Length);
SOLSTICE_API Generator<MeshData> Clothing(const Seed& SeedParam, int Type, int Fit); // Type: 0 = shirt, 1 = pants, 2 = jacket

// Character assembly
SOLSTICE_API Generator<MeshData> AssembleCharacter(
    const Generator<MeshData>& Head,
    const Generator<MeshData>& Torso,
    const Generator<MeshData>& Arms,
    const Generator<MeshData>& Legs,
    const Generator<MeshData>& Clothing,
    const Seed& SeedParam
);

} // namespace Solstice::Arzachel

