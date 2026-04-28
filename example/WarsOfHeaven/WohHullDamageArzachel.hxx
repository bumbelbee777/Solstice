#pragma once

#include <algorithm>

#include <Arzachel/AssetPresets.hxx>
#include <Arzachel/MeshData.hxx>
#include <Arzachel/Seed.hxx>

namespace Solstice::WarsOfHeaven {

/// Progressive **kinetic fragmentation** look: applies `Arzachel::Damaged` to the proc.warship mesh. `damage01` in [0,1]
/// maps to vertex displacement; combined with `MaterialGenerator::GenerateDamage` on materials in a full pipeline.
[[nodiscard]] inline Arzachel::MeshData BuildKineticFragmentedShipMesh(const Arzachel::Seed& seed, float damage01) {
    using namespace Arzachel;
    const float amount = (std::clamp)(damage01, 0.0f, 1.0f) * 1.8f;
    const Generator<MeshData> base = Ship(seed);
    const Generator<MeshData> frag = Damaged(base, seed.Derive(404u), amount);
    return frag(seed.Derive(303u));
}

} // namespace Solstice::WarsOfHeaven
